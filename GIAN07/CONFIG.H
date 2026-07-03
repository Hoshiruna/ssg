/*
 *   Config data
 *
 */

#pragma once

#include "LEVEL.H"
#include "game/midi.h"
#include "game/input.h"
#include "game/volume.h"
#include "platform/graphics_backend.h"

// フラグ //
constexpr uint8_t GRPF_PARAM_SHIFT = 3;
constexpr uint8_t GRPF_ORIG_MASK = 0x07;
constexpr uint8_t GRPF_PARAM_MASK = (
	std::to_underlying(GRAPHICS_PARAM_FLAGS::MASK) << GRPF_PARAM_SHIFT
);

// Integer promotion...
constexpr uint8_t GRPF_MASK = static_cast<uint8_t>(
	~(GRPF_ORIG_MASK | GRPF_PARAM_MASK)
);

constexpr uint8_t GRPF_WINDOW_UPPER	= 0x02;	// メッセージウィンドウを上の方に表示する
constexpr uint8_t GRPF_MSG_DISABLE	= 0x04;	// 会話シーンを省略

constexpr uint8_t SNDF_MASK	= (~0x07);
constexpr uint8_t SNDF_BGM_ENABLE	= 0x01;
constexpr uint8_t SNDF_SE_ENABLE	= 0x02;

// Normalizing the volume should be the default, so we need to store the
// negation for backwards compatibility.
constexpr uint8_t SNDF_BGM_NOT_VOL_NORM	= 0x04;

constexpr uint8_t INPF_MASK	= (~0x07);
constexpr uint8_t INPF_JOYPAD_ENABLE	= 0x01;	// パッドが有効
constexpr uint8_t INPF_Z_MSKIP_ENABLE	= 0x02;	// Ｚキーでメッセージを送れる
constexpr uint8_t INPF_Z_SPDDOWN_ENABLE	= 0x04;	// 押しっぱなしでシフト移動

constexpr uint8_t DBGF_MASK	= (~0x03);
constexpr uint8_t DBGF_DISPLAYINFO	= 0x01;	// デバッグ情報を出力する
constexpr uint8_t DBGF_HIT_ON	= 0x02;	// 当たり判定が有効

// Limits //
constexpr const auto STOCK_PLAYER_MAX = 4;
constexpr const auto STOCK_BOMB_MAX = 2;
constexpr const auto FPS_DIVISOR_MAX = 3;
constexpr const auto STAGE_MAX = 6; // ステージ数


bool ValidateAlways(auto v) { return true; };
template <typename T, T Max> constexpr bool ValidateBelow(T v) {
	return (v <= Max);
}
template <uint8_t Mask> constexpr bool ValidateMask(uint8_t v) {
	return ((v & Mask) == 0);
}
template <ENUMFLAGS Flag> constexpr bool ValidateFlag(Flag v) {
	return ((std::to_underlying(v) & ~std::to_underlying(Flag::MASK)) == 0);
}

// Option class, with optional validation of the value against a supported
// static range.
template <typename T> struct CONFIG_OPTION_VALUE {
	T v;

	// ([loaded] == std::nullopt): We loaded a config file from an earlier
	// build, where this value was not present yet. It was initialized to its
	// default value.
	// ([loaded] == [v]): Option was present in a config file, and its value
	// passed validation.
	// ([loaded] != [v]): The config file contained an invalid value, and was
	// reset to its default.
	std::optional<T> loaded = std::nullopt;

	bool (*Validate)(T);

	constexpr CONFIG_OPTION_VALUE(
		T def = T{}, bool (*validate)(T) = ValidateAlways
	) noexcept : v(def), Validate(validate)
	{
	}
};

// オプション保持用構造体 //
struct CONFIG_DATA {
	template <typename T> using OPTION = CONFIG_OPTION_VALUE<T>;

	template <class T, T V> static constexpr auto Below = &ValidateBelow<T, V>;
	template <uint8_t V> static constexpr auto U8Below = Below<uint8_t, V>;
	template <uint8_t V> static constexpr auto Mask = &ValidateMask<V>;
	template <ENUMFLAGS V> static constexpr auto Flag = &ValidateFlag<V>;
	static constexpr auto ValidateVolume = &ValidateBelow<VOLUME, VOLUME_MAX>;

		static constexpr auto ValidatePad = Below<INPUT_PAD_BUTTON, PAD_DPAD_DOWN>;

	// 難易度関連 //
	// ---------- //

	// 難易度
	OPTION<uint8_t> GameLevel = { GAME_NORMAL, U8Below<GAME_LUNATIC> };

	// 初期メイド数？
	// Quirk: Off by 2?
	OPTION<uint8_t> PlayerStock = { 2, U8Below<(STOCK_PLAYER_MAX + 2)> };

	// 初期ボム数
	// Quirk: Off by 1?
	OPTION<uint8_t> BombStock = { 2, U8Below<(STOCK_BOMB_MAX + 1)> };
	// ---------- //

	// グラフィック関連 //
	OPTION<uint8_t> DeviceID = { 0 };	// 何番目のデバイスか
	std::u8string GraphicsAPI;
	OPTION<uint8_t> WindowScale4x = { 4 };
	OPTION<int16_t> WindowLeft = { GRAPHICS_TOPLEFT_UNDEFINED };
	OPTION<int16_t> WindowTop = { GRAPHICS_TOPLEFT_UNDEFINED };
	OPTION<BITDEPTH> BitDepth = { BITDEPTHS::find(32) };	// ビット深度

	// Target frame rate = 60 / [FPSDivisor]. 0 disables any frame rate
	// limitation.
	OPTION<uint8_t> FPSDivisor = { 1, U8Below<FPS_DIVISOR_MAX> };

	// グラフィックに関するフラグ
	OPTION<uint8_t> GraphFlags = { 0, Mask<GRPF_MASK> };
	OPTION<uint8_t> ScreenshotEffort = {
		0, U8Below<GRP_SCREENSHOT_EFFORT_MAX>
	};
	OPTION<uint8_t> VSync = { 1, U8Below<1> };

	// サウンド・ＢＧＭ関連 //
	OPTION<uint8_t> SoundFlags = {
		(SNDF_SE_ENABLE | SNDF_BGM_ENABLE), Mask<SNDF_MASK>
	};
	OPTION<MID_FLAGS> MidFlags = { MID_FLAGS::FIX_SYSEX_BUGS, Flag<MID_FLAGS> };
	OPTION<VOLUME> SEVolume = { ((VOLUME_MAX * 4) / 10), ValidateVolume };
	OPTION<VOLUME> BGMVolume = { ((VOLUME_MAX * 4) / 10), ValidateVolume };
	std::u8string BGMPack;

	// 入力に関するフラグ
	OPTION<uint8_t> InputFlags = { INPF_Z_MSKIP_ENABLE, Mask<INPF_MASK> };

	// デバッグに関するフラグ
	OPTION<uint8_t> DebugFlags = { 0, Mask<DBGF_MASK> };

	OPTION<INPUT_PAD_BUTTON> PadTama = { PAD_DPAD_RIGHT, ValidatePad };
	OPTION<INPUT_PAD_BUTTON> PadBomb = { PAD_DPAD_UP, ValidatePad };
	OPTION<INPUT_PAD_BUTTON> PadShift = { PAD_DPAD_DOWN, ValidatePad };
	OPTION<INPUT_PAD_BUTTON> PadCancel = { PAD_BUTTON_NONE, ValidatePad };
	OPTION<INPUT_PAD_BUTTON> PadLeft = { PAD_LSTICK_LEFT, ValidatePad };
	OPTION<INPUT_PAD_BUTTON> PadRight = { PAD_LSTICK_RIGHT, ValidatePad };
	OPTION<INPUT_PAD_BUTTON> PadUp = { PAD_LSTICK_UP, ValidatePad };
	OPTION<INPUT_PAD_BUTTON> PadDown = { PAD_LSTICK_DOWN, ValidatePad };
	OPTION<INPUT_PAD_BUTTON> PadPause = { PAD_BUTTON_FIRST + 2, ValidatePad };

	// SDL scancodes used by the configurable keyboard bindings.
	static constexpr auto ValidateKey = Below<INPUT_KEY, 511>;
	OPTION<INPUT_KEY> KeyLeft = { 80, ValidateKey };    // Left arrow
	OPTION<INPUT_KEY> KeyRight = { 79, ValidateKey };   // Right arrow
	OPTION<INPUT_KEY> KeyUp = { 82, ValidateKey };      // Up arrow
	OPTION<INPUT_KEY> KeyDown = { 81, ValidateKey };    // Down arrow
	OPTION<INPUT_KEY> KeyShot = { 29, ValidateKey };    // Z
	OPTION<INPUT_KEY> KeyBomb = { 27, ValidateKey };    // X
	OPTION<INPUT_KEY> KeyFocus = { 225, ValidateKey };  // Left Shift
	OPTION<INPUT_KEY> KeyPause = { 41, ValidateKey };   // Escape

	// エキストラステージ判定用フラグ
	OPTION<uint8_t> ExtraStgFlags = { 0 };

	// ワケ有りでここにいるのです(チェックサムの範囲外)
	OPTION<uint8_t> StageSelect = { 0, U8Below<STAGE_MAX> };

	GRAPHICS_PARAMS GraphicsParams(void) const;
	void GraphicsParamsApply(const GRAPHICS_PARAMS& params);
};

// Active configuration
extern CONFIG_DATA ConfigDat;

#ifdef PBG_DEBUG
// デバッグ情報管理用構造体 //
typedef struct tagDEBUG_DATA{
	int32_t	MsgDisplay;	// デバッグ情報を出力するか
	int32_t	Hit;	// 当たり判定の有・無
	int32_t	DemoSave;	// デモプレイをセーブするか

	uint8_t	StgSelect;	// ステージセレクト(開始ステージ)
} DEBUG_DATA;

extern DEBUG_DATA DebugDat;
#endif

///// [関数] /////

extern void ConfigLoad();

extern void ConfigSave(void);	// コンフィグの内容を保存する
