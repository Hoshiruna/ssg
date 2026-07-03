#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <mmsystem.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(linker, \
	"\"/manifestdependency:type='win32'" \
	" name='Microsoft.Windows.Common-Controls'" \
	" version='6.0.0.0'" \
	" processorArchitecture='*'" \
	" publicKeyToken='6595b64144ccf1df'" \
	" language='*'\"" \
)

namespace {

constexpr wchar_t CONFIG_FILENAME[] = L"SSG_V03.CFG";
constexpr wchar_t LAUNCHER_INI[] = L"GIAN07_launcher.ini";
constexpr wchar_t DEFAULT_GAME_EXE[] = L"GIAN07.exe";

constexpr uint8_t GRAPH_FULLSCREEN = 0x08;
constexpr uint8_t GRAPH_EXCLUSIVE = 0x10;
constexpr uint8_t GRAPH_SCALE_GEOMETRY = 0x80;

constexpr uint8_t SOUND_BGM = 0x01;
constexpr uint8_t SOUND_SE = 0x02;
constexpr uint8_t SOUND_NO_NORMALIZE = 0x04;

constexpr uint8_t INPUT_GAMEPAD = 0x01;
constexpr uint8_t INPUT_Z_MESSAGE_SKIP = 0x02;
constexpr uint8_t INPUT_Z_FOCUS = 0x04;

HINSTANCE Instance;
std::array<HWND, 3> Pages;
std::filesystem::path LauncherDirectory;
std::filesystem::path IniPath;
std::wstring GameExecutable;
std::vector<std::string> GraphicsAPIs;

struct READER {
	const std::vector<uint8_t>& data;
	size_t pos = 0;

	bool U8(uint8_t& value)
	{
		if(pos >= data.size()) {
			return false;
		}
		value = data[pos++];
		return true;
	}

	bool I16BE(int16_t& value)
	{
		if((data.size() - pos) < 2) {
			return false;
		}
		const auto raw = static_cast<uint16_t>(
			(static_cast<uint16_t>(data[pos]) << 8) |
			static_cast<uint16_t>(data[pos + 1])
		);
		value = static_cast<int16_t>(raw);
		pos += 2;
		return true;
	}

	bool U32BE(uint32_t& value)
	{
		if((data.size() - pos) < 4) {
			return false;
		}
		value = (
			(static_cast<uint32_t>(data[pos + 0]) << 24) |
			(static_cast<uint32_t>(data[pos + 1]) << 16) |
			(static_cast<uint32_t>(data[pos + 2]) << 8) |
			(static_cast<uint32_t>(data[pos + 3]) << 0)
		);
		pos += 4;
		return true;
	}

	bool String(std::string& value)
	{
		uint32_t size;
		if(!U32BE(size) || (size > (1024 * 1024)) || (size > (data.size() - pos))) {
			return false;
		}
		value.assign(
			reinterpret_cast<const char *>(data.data() + pos),
			static_cast<size_t>(size)
		);
		pos += size;
		return true;
	}
};

struct WRITER {
	std::vector<uint8_t> data;

	void U8(uint8_t value)
	{
		data.push_back(value);
	}

	void I16BE(int16_t value)
	{
		const auto raw = static_cast<uint16_t>(value);
		data.push_back(static_cast<uint8_t>(raw >> 8));
		data.push_back(static_cast<uint8_t>(raw));
	}

	void U32BE(uint32_t value)
	{
		data.push_back(static_cast<uint8_t>(value >> 24));
		data.push_back(static_cast<uint8_t>(value >> 16));
		data.push_back(static_cast<uint8_t>(value >> 8));
		data.push_back(static_cast<uint8_t>(value));
	}

	void String(const std::string& value)
	{
		U32BE(static_cast<uint32_t>(value.size()));
		data.insert(data.end(), value.begin(), value.end());
	}
};

struct CONFIG {
	uint8_t game_level = 1;
	uint8_t player_stock = 2;
	uint8_t bomb_stock = 2;
	uint8_t device_id = 0;
	uint8_t bit_depth = 32;
	uint8_t fps_divisor = 1;
	uint8_t graph_flags = 0;
	uint8_t sound_flags = SOUND_BGM | SOUND_SE;
	uint8_t input_flags = INPUT_Z_MESSAGE_SKIP;
	uint8_t debug_flags = 0;
	uint8_t pad_shot = 1;
	uint8_t pad_bomb = 2;
	uint8_t pad_focus = 0;
	uint8_t pad_cancel = 0;
	uint8_t extra_stage_flags = 0;
	uint8_t stage_select = 0;
	uint8_t se_volume = 51;
	uint8_t bgm_volume = 51;
	std::string bgm_pack;
	uint8_t midi_flags = 1;
	std::string graphics_api;
	uint8_t window_scale_4x = 0;
	int16_t window_left = std::numeric_limits<int16_t>::min();
	int16_t window_top = std::numeric_limits<int16_t>::min();
	uint8_t screenshot_effort = 0;

	bool Load(const std::filesystem::path& path)
	{
		std::ifstream stream(path, std::ios::binary | std::ios::ate);
		if(!stream) {
			return false;
		}
		const auto end = stream.tellg();
		if((end <= 0) || (end > (16 * 1024 * 1024))) {
			return false;
		}
		std::vector<uint8_t> bytes(static_cast<size_t>(end));
		stream.seekg(0);
		if(!stream.read(
			reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		)) {
			return false;
		}

		CONFIG loaded;
		READER reader{ bytes };
		if(
			!reader.U8(loaded.game_level) ||
			!reader.U8(loaded.player_stock) ||
			!reader.U8(loaded.bomb_stock) ||
			!reader.U8(loaded.device_id) ||
			!reader.U8(loaded.bit_depth) ||
			!reader.U8(loaded.fps_divisor) ||
			!reader.U8(loaded.graph_flags) ||
			!reader.U8(loaded.sound_flags) ||
			!reader.U8(loaded.input_flags) ||
			!reader.U8(loaded.debug_flags) ||
			!reader.U8(loaded.pad_shot) ||
			!reader.U8(loaded.pad_bomb) ||
			!reader.U8(loaded.pad_focus) ||
			!reader.U8(loaded.pad_cancel) ||
			!reader.U8(loaded.extra_stage_flags) ||
			!reader.U8(loaded.stage_select) ||
			!reader.U8(loaded.se_volume) ||
			!reader.U8(loaded.bgm_volume) ||
			!reader.String(loaded.bgm_pack) ||
			!reader.U8(loaded.midi_flags) ||
			!reader.String(loaded.graphics_api) ||
			!reader.U8(loaded.window_scale_4x) ||
			!reader.I16BE(loaded.window_left) ||
			!reader.I16BE(loaded.window_top) ||
			!reader.U8(loaded.screenshot_effort)
		) {
			return false;
		}
		*this = std::move(loaded);
		return true;
	}

	bool Save(const std::filesystem::path& path) const
	{
		WRITER writer;
		writer.U8(game_level);
		writer.U8(player_stock);
		writer.U8(bomb_stock);
		writer.U8(device_id);
		writer.U8(bit_depth);
		writer.U8(fps_divisor);
		writer.U8(graph_flags);
		writer.U8(sound_flags);
		writer.U8(input_flags);
		writer.U8(debug_flags);
		writer.U8(pad_shot);
		writer.U8(pad_bomb);
		writer.U8(pad_focus);
		writer.U8(pad_cancel);
		writer.U8(extra_stage_flags);
		writer.U8(stage_select);
		writer.U8(se_volume);
		writer.U8(bgm_volume);
		writer.String(bgm_pack);
		writer.U8(midi_flags);
		writer.String(graphics_api);
		writer.U8(window_scale_4x);
		writer.I16BE(window_left);
		writer.I16BE(window_top);
		writer.U8(screenshot_effort);

		auto temporary = path;
		temporary += L".tmp";
		{
			std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
			if(
				!stream ||
				!stream.write(
					reinterpret_cast<const char *>(writer.data.data()),
					static_cast<std::streamsize>(writer.data.size())
				)
			) {
				return false;
			}
		}
		if(!MoveFileExW(
			temporary.c_str(),
			path.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
		)) {
			DeleteFileW(temporary.c_str());
			return false;
		}
		return true;
	}
} Config;

std::wstring UTF8ToWide(const std::string_view value)
{
	if(value.empty()) {
		return {};
	}
	const auto size = MultiByteToWideChar(
		CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
		static_cast<int>(value.size()), nullptr, 0
	);
	if(size <= 0) {
		return {};
	}
	std::wstring ret(static_cast<size_t>(size), L'\0');
	MultiByteToWideChar(
		CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
		static_cast<int>(value.size()), ret.data(), size
	);
	return ret;
}

std::string WideToUTF8(const std::wstring_view value)
{
	if(value.empty()) {
		return {};
	}
	const auto size = WideCharToMultiByte(
		CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
		static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr
	);
	if(size <= 0) {
		return {};
	}
	std::string ret(static_cast<size_t>(size), '\0');
	WideCharToMultiByte(
		CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
		static_cast<int>(value.size()), ret.data(), size, nullptr, nullptr
	);
	return ret;
}

std::wstring DialogText(HWND dialog, int id)
{
	const auto length = GetWindowTextLengthW(GetDlgItem(dialog, id));
	std::wstring ret(static_cast<size_t>(length) + 1, L'\0');
	GetDlgItemTextW(dialog, id, ret.data(), static_cast<int>(ret.size()));
	ret.resize(static_cast<size_t>(length));
	return ret;
}

void SetCheck(HWND dialog, int id, bool checked)
{
	CheckDlgButton(dialog, id, checked ? BST_CHECKED : BST_UNCHECKED);
}

bool IsChecked(HWND dialog, int id)
{
	return (IsDlgButtonChecked(dialog, id) == BST_CHECKED);
}

void SelectComboData(HWND combo, LPARAM wanted)
{
	const auto count = static_cast<int>(SendMessageW(combo, CB_GETCOUNT, 0, 0));
	for(int i = 0; i < count; i++) {
		if(SendMessageW(combo, CB_GETITEMDATA, i, 0) == wanted) {
			SendMessageW(combo, CB_SETCURSEL, i, 0);
			return;
		}
	}
	if(count > 0) {
		SendMessageW(combo, CB_SETCURSEL, 0, 0);
	}
}

LPARAM SelectedComboData(HWND combo, LPARAM fallback)
{
	const auto selected = SendMessageW(combo, CB_GETCURSEL, 0, 0);
	if(selected == CB_ERR) {
		return fallback;
	}
	const auto data = SendMessageW(combo, CB_GETITEMDATA, selected, 0);
	return (data == CB_ERR) ? fallback : data;
}

std::filesystem::path ResolvedGamePath()
{
	std::filesystem::path path(GameExecutable);
	if(path.is_relative()) {
		path = LauncherDirectory / path;
	}
	return path.lexically_normal();
}

std::filesystem::path ConfigPath()
{
	auto directory = ResolvedGamePath().parent_path();
	if(directory.empty()) {
		directory = LauncherDirectory;
	}
	return directory / CONFIG_FILENAME;
}

void AddComboItem(HWND combo, const wchar_t *label, LPARAM data)
{
	const auto index = SendMessageW(combo, CB_ADDSTRING, 0, LPARAM(label));
	if(index != CB_ERR) {
		SendMessageW(combo, CB_SETITEMDATA, index, data);
	}
}

INT_PTR CALLBACK DisplayProc(HWND dialog, UINT message, WPARAM, LPARAM)
{
	if(message != WM_INITDIALOG) {
		return FALSE;
	}

	struct SCALE {
		uint8_t value;
		const wchar_t *label;
	};
	static constexpr SCALE scales[] = {
		{ 0, L"Fit to display" },
		{ 1, L"160x120 (0.25x)" },
		{ 2, L"320x240 (0.5x)" },
		{ 3, L"480x360 (0.75x)" },
		{ 4, L"640x480 (1x)" },
		{ 5, L"800x600 (1.25x)" },
		{ 6, L"960x720 (1.5x)" },
		{ 7, L"1120x840 (1.75x)" },
		{ 8, L"1280x960 (2x)" },
		{ 9, L"1440x1080 (2.25x)" },
		{ 10, L"1600x1200 (2.5x)" },
		{ 11, L"1760x1320 (2.75x)" },
		{ 12, L"1920x1440 (3x)" },
	};
	const auto window_size = GetDlgItem(dialog, IDC_WINDOW_SIZE);
	for(const auto& scale : scales) {
		AddComboItem(window_size, scale.label, scale.value);
	}
	SelectComboData(window_size, Config.window_scale_4x);

	struct API {
		const char *value;
		const wchar_t *label;
	};
	static constexpr API apis[] = {
		{ "", L"Default (Recommended)" },
		{ "gpu", L"SDL GPU" },
		{ "direct3d12", L"Direct3D 12" },
		{ "direct3d11", L"Direct3D 11" },
		{ "direct3d", L"Direct3D 9" },
		{ "opengl", L"OpenGL" },
		{ "opengles2", L"OpenGL ES 2" },
		{ "software", L"Software" },
	};
	const auto api_combo = GetDlgItem(dialog, IDC_GRAPHICS_API);
	GraphicsAPIs.clear();
	int api_selected = 0;
	for(const auto& api : apis) {
		GraphicsAPIs.emplace_back(api.value);
		AddComboItem(api_combo, api.label, GraphicsAPIs.size() - 1);
		if(Config.graphics_api == api.value) {
			api_selected = static_cast<int>(GraphicsAPIs.size() - 1);
		}
	}
	if(
		!Config.graphics_api.empty() &&
		(std::ranges::find(GraphicsAPIs, Config.graphics_api) == GraphicsAPIs.end())
	) {
		GraphicsAPIs.push_back(Config.graphics_api);
		const auto custom = L"Custom (" + UTF8ToWide(Config.graphics_api) + L")";
		AddComboItem(api_combo, custom.c_str(), GraphicsAPIs.size() - 1);
		api_selected = static_cast<int>(GraphicsAPIs.size() - 1);
	}
	SendMessageW(api_combo, CB_SETCURSEL, api_selected, 0);

	CheckRadioButton(
		dialog, IDC_FULLSCREEN, IDC_WINDOWED,
		(Config.graph_flags & GRAPH_FULLSCREEN) ? IDC_FULLSCREEN : IDC_WINDOWED
	);
	SetCheck(dialog, IDC_BORDERLESS, !(Config.graph_flags & GRAPH_EXCLUSIVE));
	SetCheck(
		dialog, IDC_SCALE_GEOMETRY,
		!!(Config.graph_flags & GRAPH_SCALE_GEOMETRY)
	);

	const auto bpp = (
		(Config.bit_depth == 8) ? IDC_BPP_8 :
		(Config.bit_depth == 16) ? IDC_BPP_16 :
		IDC_BPP_32
	);
	CheckRadioButton(dialog, IDC_BPP_32, IDC_BPP_8, bpp);

	const auto fps = (
		(Config.fps_divisor == 0) ? IDC_FPS_UNLIMITED :
		(Config.fps_divisor == 2) ? IDC_FPS_HALF :
		(Config.fps_divisor == 3) ? IDC_FPS_THIRD :
		IDC_FPS_NONE
	);
	CheckRadioButton(dialog, IDC_FPS_NONE, IDC_FPS_UNLIMITED, fps);
	return TRUE;
}

void CaptureDisplay()
{
	const auto dialog = Pages[0];
	Config.window_scale_4x = static_cast<uint8_t>(SelectedComboData(
		GetDlgItem(dialog, IDC_WINDOW_SIZE), Config.window_scale_4x
	));

	Config.graph_flags &= ~(
		GRAPH_FULLSCREEN | GRAPH_EXCLUSIVE | GRAPH_SCALE_GEOMETRY
	);
	if(IsChecked(dialog, IDC_FULLSCREEN)) {
		Config.graph_flags |= GRAPH_FULLSCREEN;
	}
	if(!IsChecked(dialog, IDC_BORDERLESS)) {
		Config.graph_flags |= GRAPH_EXCLUSIVE;
	}
	if(IsChecked(dialog, IDC_SCALE_GEOMETRY)) {
		Config.graph_flags |= GRAPH_SCALE_GEOMETRY;
	}

	const auto api_index = static_cast<size_t>(SelectedComboData(
		GetDlgItem(dialog, IDC_GRAPHICS_API), 0
	));
	Config.graphics_api = (
		(api_index < GraphicsAPIs.size()) ? GraphicsAPIs[api_index] : ""
	);

	Config.bit_depth = (
		IsChecked(dialog, IDC_BPP_8) ? 8 :
		IsChecked(dialog, IDC_BPP_16) ? 16 :
		32
	);
	Config.fps_divisor = (
		IsChecked(dialog, IDC_FPS_UNLIMITED) ? 0 :
		IsChecked(dialog, IDC_FPS_HALF) ? 2 :
		IsChecked(dialog, IDC_FPS_THIRD) ? 3 :
		1
	);
}

constexpr std::array KeyControls = {
	IDC_KEY_HEADER_ACTION, IDC_KEY_HEADER_KEYBOARD, IDC_KEY_HEADER_PAD,
	IDC_ACTION_LEFT, IDC_ACTION_RIGHT, IDC_ACTION_UP, IDC_ACTION_DOWN,
	IDC_ACTION_SHOT, IDC_ACTION_BOMB, IDC_ACTION_FOCUS, IDC_ACTION_CANCEL,
	IDC_KEY_LEFT, IDC_KEY_RIGHT, IDC_KEY_UP, IDC_KEY_DOWN,
	IDC_KEY_SHOT, IDC_KEY_BOMB, IDC_KEY_FOCUS, IDC_KEY_CANCEL,
	IDC_PAD_LEFT, IDC_PAD_RIGHT, IDC_PAD_UP, IDC_PAD_DOWN,
	IDC_PAD_SHOT, IDC_PAD_BOMB, IDC_PAD_FOCUS, IDC_PAD_CANCEL,
};
constexpr std::array DeviceControls = {
	IDC_DEVICE_LIST, IDC_DEVICE_DETAILS, IDC_DEVICE_HELP,
};
constexpr std::array TestControls = {
	IDC_TEST_HELP, IDC_TEST_X, IDC_TEST_Y, IDC_TEST_BUTTONS,
};

void ShowControls(HWND dialog, const auto& controls, bool show)
{
	for(const auto id : controls) {
		ShowWindow(GetDlgItem(dialog, id), show ? SW_SHOW : SW_HIDE);
	}
}

std::optional<UINT> SelectedPad(HWND dialog)
{
	const auto combo = GetDlgItem(dialog, IDC_PAD_DEVICES);
	const auto selected = SendMessageW(combo, CB_GETCURSEL, 0, 0);
	if(selected == CB_ERR) {
		return std::nullopt;
	}
	const auto data = SendMessageW(combo, CB_GETITEMDATA, selected, 0);
	if((data == CB_ERR) || (data == std::numeric_limits<UINT>::max())) {
		return std::nullopt;
	}
	return static_cast<UINT>(data);
}

void UpdateDeviceDetails(HWND dialog)
{
	const auto maybe_id = SelectedPad(dialog);
	if(!maybe_id) {
		SetDlgItemTextW(
			dialog, IDC_DEVICE_DETAILS,
			L"No controller is selected."
		);
		return;
	}
	JOYCAPSW caps{};
	if(joyGetDevCapsW(*maybe_id, &caps, sizeof(caps)) != JOYERR_NOERROR) {
		SetDlgItemTextW(
			dialog, IDC_DEVICE_DETAILS,
			L"The selected controller is no longer available."
		);
		return;
	}
	wchar_t text[256];
	swprintf_s(
		text,
		L"%s\r\n%u axes, %u buttons",
		caps.szPname, caps.wNumAxes, caps.wNumButtons
	);
	SetDlgItemTextW(dialog, IDC_DEVICE_DETAILS, text);
}

void RefreshDevices(HWND dialog)
{
	const auto combo = GetDlgItem(dialog, IDC_PAD_DEVICES);
	const auto list = GetDlgItem(dialog, IDC_DEVICE_LIST);
	const auto previous = SelectedPad(dialog);
	SendMessageW(combo, CB_RESETCONTENT, 0, 0);
	SendMessageW(list, LB_RESETCONTENT, 0, 0);

	int selected = -1;
	const auto count = joyGetNumDevs();
	for(UINT id = 0; id < count; id++) {
		JOYINFOEX state{ .dwSize = sizeof(state), .dwFlags = JOY_RETURNALL };
		JOYCAPSW caps{};
		if(
			(joyGetPosEx(id, &state) != JOYERR_NOERROR) ||
			(joyGetDevCapsW(id, &caps, sizeof(caps)) != JOYERR_NOERROR)
		) {
			continue;
		}
		wchar_t label[256];
		swprintf_s(label, L"%s (Joystick %u)", caps.szPname, id + 1);
		const auto index = SendMessageW(combo, CB_ADDSTRING, 0, LPARAM(label));
		SendMessageW(combo, CB_SETITEMDATA, index, id);
		SendMessageW(list, LB_ADDSTRING, 0, LPARAM(label));
		if(previous && (*previous == id)) {
			selected = static_cast<int>(index);
		}
	}

	if(SendMessageW(combo, CB_GETCOUNT, 0, 0) == 0) {
		const auto index = SendMessageW(
			combo, CB_ADDSTRING, 0, LPARAM(L"(No pad device detected)")
		);
		SendMessageW(
			combo, CB_SETITEMDATA, index, std::numeric_limits<UINT>::max()
		);
		SendMessageW(
			list, LB_ADDSTRING, 0, LPARAM(L"No controller detected.")
		);
		selected = 0;
	} else if(selected < 0) {
		selected = 0;
	}
	SendMessageW(combo, CB_SETCURSEL, selected, 0);
	UpdateDeviceDetails(dialog);
}

void UpdateControllerTest(HWND dialog)
{
	const auto maybe_id = SelectedPad(dialog);
	if(!maybe_id) {
		SetDlgItemTextW(dialog, IDC_TEST_X, L"X Axis: —");
		SetDlgItemTextW(dialog, IDC_TEST_Y, L"Y Axis: —");
		SetDlgItemTextW(dialog, IDC_TEST_BUTTONS, L"Buttons: None");
		return;
	}
	JOYINFOEX state{ .dwSize = sizeof(state), .dwFlags = JOY_RETURNALL };
	if(joyGetPosEx(*maybe_id, &state) != JOYERR_NOERROR) {
		SetDlgItemTextW(dialog, IDC_TEST_X, L"X Axis: unavailable");
		SetDlgItemTextW(dialog, IDC_TEST_Y, L"Y Axis: unavailable");
		SetDlgItemTextW(dialog, IDC_TEST_BUTTONS, L"Buttons: unavailable");
		return;
	}
	wchar_t axis[64];
	swprintf_s(axis, L"X Axis: %lu", state.dwXpos);
	SetDlgItemTextW(dialog, IDC_TEST_X, axis);
	swprintf_s(axis, L"Y Axis: %lu", state.dwYpos);
	SetDlgItemTextW(dialog, IDC_TEST_Y, axis);

	std::wstring buttons = L"Buttons: ";
	bool any = false;
	for(unsigned int bit = 0; bit < 32; bit++) {
		if(state.dwButtons & (1u << bit)) {
			if(any) {
				buttons += L", ";
			}
			buttons += std::to_wstring(bit + 1);
			any = true;
		}
	}
	if(!any) {
		buttons += L"None";
	}
	SetDlgItemTextW(dialog, IDC_TEST_BUTTONS, buttons.c_str());
}

void UpdateInputSubtab(HWND dialog)
{
	auto selected = static_cast<int>(SendDlgItemMessageW(
		dialog, IDC_INPUT_SUBTAB, TCM_GETCURSEL, 0, 0
	));
	if(selected < 0) {
		selected = 0;
	}
	ShowControls(dialog, KeyControls, selected == 0);
	ShowControls(dialog, DeviceControls, selected == 1);
	ShowControls(dialog, TestControls, selected == 2);
	if(selected == 1) {
		UpdateDeviceDetails(dialog);
	} else if(selected == 2) {
		UpdateControllerTest(dialog);
	}
}

void PopulateButtonCombo(HWND combo, uint8_t selected)
{
	AddComboItem(combo, L"Not assigned", 0);
	for(uint8_t button = 1; button <= 32; button++) {
		wchar_t label[32];
		swprintf_s(label, L"Button %u", button);
		AddComboItem(combo, label, button);
	}
	SelectComboData(combo, selected);
}

INT_PTR CALLBACK InputProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch(message) {
	case WM_INITDIALOG: {
		const auto tab = GetDlgItem(dialog, IDC_INPUT_SUBTAB);
		for(const auto *label : {
			L"Key Config", L"Input Devices", L"Controller Test"
		}) {
			TCITEMW item{ .mask = TCIF_TEXT, .pszText = const_cast<wchar_t *>(label) };
			TabCtrl_InsertItem(tab, TabCtrl_GetItemCount(tab), &item);
		}
		SetDlgItemTextW(dialog, IDC_KEY_LEFT, L"Left Arrow");
		SetDlgItemTextW(dialog, IDC_KEY_RIGHT, L"Right Arrow");
		SetDlgItemTextW(dialog, IDC_KEY_UP, L"Up Arrow");
		SetDlgItemTextW(dialog, IDC_KEY_DOWN, L"Down Arrow");
		SetDlgItemTextW(dialog, IDC_KEY_SHOT, L"Z / Enter");
		SetDlgItemTextW(dialog, IDC_KEY_BOMB, L"X");
		SetDlgItemTextW(dialog, IDC_KEY_FOCUS, L"Left Shift");
		SetDlgItemTextW(dialog, IDC_KEY_CANCEL, L"Escape");
		SetDlgItemTextW(dialog, IDC_PAD_LEFT, L"X Axis -");
		SetDlgItemTextW(dialog, IDC_PAD_RIGHT, L"X Axis +");
		SetDlgItemTextW(dialog, IDC_PAD_UP, L"Y Axis -");
		SetDlgItemTextW(dialog, IDC_PAD_DOWN, L"Y Axis +");
		PopulateButtonCombo(GetDlgItem(dialog, IDC_PAD_SHOT), Config.pad_shot);
		PopulateButtonCombo(GetDlgItem(dialog, IDC_PAD_BOMB), Config.pad_bomb);
		PopulateButtonCombo(GetDlgItem(dialog, IDC_PAD_FOCUS), Config.pad_focus);
		PopulateButtonCombo(GetDlgItem(dialog, IDC_PAD_CANCEL), Config.pad_cancel);
		RefreshDevices(dialog);
		UpdateInputSubtab(dialog);
		SetTimer(dialog, 1, 50, nullptr);
		return TRUE;
	}

	case WM_COMMAND:
		if(LOWORD(wparam) == IDC_REFRESH_DEVICES) {
			RefreshDevices(dialog);
			return TRUE;
		}
		if(
			(LOWORD(wparam) == IDC_PAD_DEVICES) &&
			(HIWORD(wparam) == CBN_SELCHANGE)
		) {
			UpdateDeviceDetails(dialog);
			UpdateControllerTest(dialog);
			return TRUE;
		}
		break;

	case WM_NOTIFY:
		if(
			(wparam == IDC_INPUT_SUBTAB) &&
			(reinterpret_cast<NMHDR *>(lparam)->code == TCN_SELCHANGE)
		) {
			UpdateInputSubtab(dialog);
			return TRUE;
		}
		break;

	case WM_TIMER:
		if(
			SendDlgItemMessageW(
				dialog, IDC_INPUT_SUBTAB, TCM_GETCURSEL, 0, 0
			) == 2
		) {
			UpdateControllerTest(dialog);
		}
		return TRUE;

	case WM_DESTROY:
		KillTimer(dialog, 1);
		break;
	}
	return FALSE;
}

void CaptureInput()
{
	const auto dialog = Pages[1];
	Config.pad_shot = static_cast<uint8_t>(SelectedComboData(
		GetDlgItem(dialog, IDC_PAD_SHOT), Config.pad_shot
	));
	Config.pad_bomb = static_cast<uint8_t>(SelectedComboData(
		GetDlgItem(dialog, IDC_PAD_BOMB), Config.pad_bomb
	));
	Config.pad_focus = static_cast<uint8_t>(SelectedComboData(
		GetDlgItem(dialog, IDC_PAD_FOCUS), Config.pad_focus
	));
	Config.pad_cancel = static_cast<uint8_t>(SelectedComboData(
		GetDlgItem(dialog, IDC_PAD_CANCEL), Config.pad_cancel
	));
}

void BrowseForGame(HWND dialog)
{
	std::array<wchar_t, 32768> filename{};
	const auto current = DialogText(dialog, IDC_GAME_PATH);
	wcsncpy_s(filename.data(), filename.size(), current.c_str(), _TRUNCATE);

	OPENFILENAMEW ofn{
		.lStructSize = sizeof(ofn),
		.hwndOwner = dialog,
		.lpstrFilter = L"Executable Files (*.exe)\0*.exe\0All Files (*.*)\0*.*\0",
		.lpstrFile = filename.data(),
		.nMaxFile = static_cast<DWORD>(filename.size()),
		.lpstrTitle = L"Select the game executable",
		.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR,
		.lpstrDefExt = L"exe",
	};
	if(GetOpenFileNameW(&ofn)) {
		SetDlgItemTextW(dialog, IDC_GAME_PATH, filename.data());
	}
}

INT_PTR CALLBACK OptionProc(HWND dialog, UINT message, WPARAM wparam, LPARAM)
{
	switch(message) {
	case WM_INITDIALOG: {
		SetDlgItemTextW(dialog, IDC_GAME_PATH, GameExecutable.c_str());
		SetCheck(dialog, IDC_ENABLE_BGM, !!(Config.sound_flags & SOUND_BGM));
		SetCheck(dialog, IDC_ENABLE_SE, !!(Config.sound_flags & SOUND_SE));
		SetCheck(
			dialog, IDC_NORMALIZE_BGM,
			!(Config.sound_flags & SOUND_NO_NORMALIZE)
		);
		SetCheck(
			dialog, IDC_ENABLE_GAMEPAD,
			!!(Config.input_flags & INPUT_GAMEPAD)
		);
		SetCheck(
			dialog, IDC_Z_MESSAGE_SKIP,
			!!(Config.input_flags & INPUT_Z_MESSAGE_SKIP)
		);
		SetCheck(
			dialog, IDC_Z_FOCUS,
			!!(Config.input_flags & INPUT_Z_FOCUS)
		);

		const auto screenshot = GetDlgItem(dialog, IDC_SCREENSHOT_FORMAT);
		AddComboItem(screenshot, L"BMP (Fast, lossless)", 0);
		for(uint8_t effort = 1; effort <= 10; effort++) {
			wchar_t label[64];
			swprintf_s(
				label, L"WebP lossless (Compression effort %u)", effort - 1
			);
			AddComboItem(screenshot, label, effort);
		}
		SelectComboData(screenshot, Config.screenshot_effort);
		return TRUE;
	}

	case WM_COMMAND:
		if(LOWORD(wparam) == IDC_BROWSE_GAME) {
			BrowseForGame(dialog);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void CaptureOption()
{
	const auto dialog = Pages[2];
	GameExecutable = DialogText(dialog, IDC_GAME_PATH);

	Config.sound_flags &= ~(SOUND_BGM | SOUND_SE | SOUND_NO_NORMALIZE);
	if(IsChecked(dialog, IDC_ENABLE_BGM)) {
		Config.sound_flags |= SOUND_BGM;
	}
	if(IsChecked(dialog, IDC_ENABLE_SE)) {
		Config.sound_flags |= SOUND_SE;
	}
	if(!IsChecked(dialog, IDC_NORMALIZE_BGM)) {
		Config.sound_flags |= SOUND_NO_NORMALIZE;
	}

	Config.input_flags &= ~(
		INPUT_GAMEPAD | INPUT_Z_MESSAGE_SKIP | INPUT_Z_FOCUS
	);
	if(IsChecked(dialog, IDC_ENABLE_GAMEPAD)) {
		Config.input_flags |= INPUT_GAMEPAD;
	}
	if(IsChecked(dialog, IDC_Z_MESSAGE_SKIP)) {
		Config.input_flags |= INPUT_Z_MESSAGE_SKIP;
	}
	if(IsChecked(dialog, IDC_Z_FOCUS)) {
		Config.input_flags |= INPUT_Z_FOCUS;
	}
	Config.screenshot_effort = static_cast<uint8_t>(SelectedComboData(
		GetDlgItem(dialog, IDC_SCREENSHOT_FORMAT), Config.screenshot_effort
	));
}

void CaptureAll()
{
	CaptureDisplay();
	CaptureInput();
	CaptureOption();
}

void ShowError(HWND owner, const wchar_t *message)
{
	MessageBoxW(owner, message, L"秋霜玉 Launcher", MB_OK | MB_ICONERROR);
}

bool SaveAll(HWND owner)
{
	CaptureAll();
	if(GameExecutable.empty()) {
		ShowError(owner, L"Select a game executable before saving.");
		return false;
	}

	const auto config_path = ConfigPath();
	std::error_code ec;
	if(!std::filesystem::is_directory(config_path.parent_path(), ec)) {
		ShowError(owner, L"The selected game directory does not exist.");
		return false;
	}
	if(!Config.Save(config_path)) {
		ShowError(owner, L"Could not save SSG_V03.CFG beside the game executable.");
		return false;
	}
	if(!WritePrivateProfileStringW(
		L"Launcher", L"GameExecutable", GameExecutable.c_str(), IniPath.c_str()
	)) {
		ShowError(owner, L"Could not save the launcher settings.");
		return false;
	}
	return true;
}

bool StartGame(HWND owner)
{
	if(!SaveAll(owner)) {
		return false;
	}
	const auto executable = ResolvedGamePath();
	std::error_code ec;
	if(!std::filesystem::is_regular_file(executable, ec)) {
		ShowError(owner, L"The selected game executable does not exist.");
		return false;
	}

	auto command = L"\"" + executable.wstring() + L"\"";
	std::vector<wchar_t> command_buffer(command.begin(), command.end());
	command_buffer.push_back(L'\0');
	const auto working_directory = executable.parent_path();
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION process{};
	if(!CreateProcessW(
		executable.c_str(),
		command_buffer.data(),
		nullptr,
		nullptr,
		FALSE,
		0,
		nullptr,
		working_directory.c_str(),
		&startup,
		&process
	)) {
		ShowError(owner, L"Windows could not start the selected game executable.");
		return false;
	}
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return true;
}

void PositionPages(HWND dialog)
{
	const auto tab = GetDlgItem(dialog, IDC_MAIN_TAB);
	RECT rect;
	GetClientRect(tab, &rect);
	TabCtrl_AdjustRect(tab, FALSE, &rect);
	MapWindowPoints(tab, dialog, reinterpret_cast<POINT *>(&rect), 2);
	for(const auto page : Pages) {
		SetWindowPos(
			page, nullptr,
			rect.left, rect.top,
			rect.right - rect.left, rect.bottom - rect.top,
			SWP_NOZORDER | SWP_NOACTIVATE
		);
	}
}

void SelectMainPage(HWND dialog)
{
	auto selected = static_cast<int>(SendDlgItemMessageW(
		dialog, IDC_MAIN_TAB, TCM_GETCURSEL, 0, 0
	));
	if((selected < 0) || (selected >= static_cast<int>(Pages.size()))) {
		selected = 0;
	}
	for(size_t i = 0; i < Pages.size(); i++) {
		ShowWindow(Pages[i], (i == static_cast<size_t>(selected)) ? SW_SHOW : SW_HIDE);
	}
}

void HandleMainCommand(HWND dialog, int id)
{
	switch(id) {
	case IDC_SAVE_EXIT:
	case IDM_SAVE_EXIT:
		if(SaveAll(dialog)) {
			EndDialog(dialog, 0);
		}
		break;

	case IDC_EXIT:
	case IDM_EXIT:
	case IDCANCEL:
		EndDialog(dialog, 0);
		break;

	case IDC_START_GAME:
	case IDM_START_GAME:
		if(StartGame(dialog)) {
			EndDialog(dialog, 0);
		}
		break;
	}
}

INT_PTR CALLBACK MainProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch(message) {
	case WM_INITDIALOG: {
		SendMessageW(
			dialog, WM_SETICON, ICON_BIG,
			LPARAM(LoadIconW(Instance, MAKEINTRESOURCEW(IDI_LAUNCHER)))
		);
		SendMessageW(
			dialog, WM_SETICON, ICON_SMALL,
			LPARAM(LoadIconW(Instance, MAKEINTRESOURCEW(IDI_LAUNCHER)))
		);

		const auto tab = GetDlgItem(dialog, IDC_MAIN_TAB);
		for(const auto *label : { L"Display", L"Input", L"Option" }) {
			TCITEMW item{ .mask = TCIF_TEXT, .pszText = const_cast<wchar_t *>(label) };
			TabCtrl_InsertItem(tab, TabCtrl_GetItemCount(tab), &item);
		}
		Pages[0] = CreateDialogParamW(
			Instance, MAKEINTRESOURCEW(IDD_DISPLAY), dialog, DisplayProc, 0
		);
		Pages[1] = CreateDialogParamW(
			Instance, MAKEINTRESOURCEW(IDD_INPUT), dialog, InputProc, 0
		);
		Pages[2] = CreateDialogParamW(
			Instance, MAKEINTRESOURCEW(IDD_OPTION), dialog, OptionProc, 0
		);
		PositionPages(dialog);
		SelectMainPage(dialog);
		return TRUE;
	}

	case WM_COMMAND:
		HandleMainCommand(dialog, LOWORD(wparam));
		return TRUE;

	case WM_NOTIFY:
		if(
			(wparam == IDC_MAIN_TAB) &&
			(reinterpret_cast<NMHDR *>(lparam)->code == TCN_SELCHANGE)
		) {
			SelectMainPage(dialog);
			return TRUE;
		}
		break;

	case WM_CLOSE:
		EndDialog(dialog, 0);
		return TRUE;
	}
	return FALSE;
}

std::filesystem::path ModuleDirectory()
{
	std::vector<wchar_t> path(32768);
	const auto length = GetModuleFileNameW(
		nullptr, path.data(), static_cast<DWORD>(path.size())
	);
	if((length == 0) || (length >= path.size())) {
		return std::filesystem::current_path();
	}
	return std::filesystem::path(
		std::wstring_view(path.data(), length)
	).parent_path();
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
	Instance = instance;
	LauncherDirectory = ModuleDirectory();
	IniPath = LauncherDirectory / LAUNCHER_INI;

	std::array<wchar_t, 32768> game_executable{};
	GetPrivateProfileStringW(
		L"Launcher",
		L"GameExecutable",
		DEFAULT_GAME_EXE,
		game_executable.data(),
		static_cast<DWORD>(game_executable.size()),
		IniPath.c_str()
	);
	GameExecutable = game_executable.data();
	Config.Load(ConfigPath());

	INITCOMMONCONTROLSEX controls{
		.dwSize = sizeof(controls),
		.dwICC = ICC_TAB_CLASSES,
	};
	InitCommonControlsEx(&controls);

	return static_cast<int>(DialogBoxParamW(
		instance, MAKEINTRESOURCEW(IDD_MAIN), nullptr, MainProc, 0
	));
}
