/*
 *   Replay and practice menu flow
 */

import std.compat;

#include "REPLAYUI.H"

#include "CONFIG.H"
#include "FONTUTY.H"
#include "GAMEMAIN.H"
#include "GIAN.H"
#include "LOADER.H"
#include "REPLAY.H"
#include "WindowCtrl.h"
#include "WindowSys.h"
#include "game/snd.h"

namespace {

enum class UI_STATE {
	NONE,
	LOAD_SLOTS,
	LOAD_STAGES,
	PRACTICE,
	SAVE_PROMPT,
	SAVE_SLOTS,
	OVERWRITE,
	NAME,
	MESSAGE,
};

UI_STATE State = UI_STATE::NONE;
void (*Done)(void) = nullptr;
size_t SelectedSlot = 0;
size_t HighlightedSlot = 0;
std::array<REPLAY_SLOT_INFO, REPLAY_SLOT_COUNT> Slots;
std::array<char, (REPLAY_NAME_LENGTH + 1)> ReplayName;
size_t NameCursor = 0;
INPUT_BITS NameOldKey = 0;
std::string Message;

constexpr size_t SLOT_VISIBLE = 10;
char SlotTitle[REPLAY_SLOT_COUNT][48];
char SlotHeader[48];
WINDOW_LABEL SlotTitleItem = { SlotHeader };

char StageTitle[REPLAY_STAGE_COUNT_MAX][32];
WINDOW_CHOICE StageItem[REPLAY_STAGE_COUNT_MAX];
WINDOW_LABEL StageTitleItem = { " Select Stage" };
WINDOW_MENU StageMenu = { std::span(StageItem), [](bool) {}, &StageTitleItem };

char PracticeTitle[REPLAY_STAGE_COUNT_MAX][32];
WINDOW_CHOICE PracticeItem[REPLAY_STAGE_COUNT_MAX];
WINDOW_LABEL PracticeTitleItem = { " Practice Start" };
WINDOW_MENU PracticeMenu = {
	std::span(PracticeItem), [](bool) {}, &PracticeTitleItem
};

static bool PromptYes(INPUT_BITS key);
static bool PromptNo(INPUT_BITS key);
WINDOW_LABEL PromptTitle = { " Save Replay?" };
WINDOW_CHOICE PromptItem[] = {
	{ " Yes", "Save this run to one of 20 replay slots", PromptYes },
	{ " No", "Continue without saving this replay", PromptNo },
};
WINDOW_MENU PromptMenu = { std::span(PromptItem), [](bool) {}, &PromptTitle };

static bool OverwriteYes(INPUT_BITS key);
static bool OverwriteNo(INPUT_BITS key);
WINDOW_LABEL OverwriteTitle = { " Overwrite Replay?" };
WINDOW_CHOICE OverwriteItem[] = {
	{ " Yes", "Replace the replay currently in this slot", OverwriteYes },
	{ " No", "Return to the replay slot list", OverwriteNo },
};
WINDOW_MENU OverwriteMenu = {
	std::span(OverwriteItem), [](bool) {}, &OverwriteTitle
};

static bool MessageOK(INPUT_BITS key);
WINDOW_LABEL MessageTitle = { " Replay" };
WINDOW_CHOICE MessageItem[] = {
	{ " OK", "Close this message", MessageOK },
};
WINDOW_MENU MessageMenu = { std::span(MessageItem), [](bool) {}, &MessageTitle };

extern WINDOW_SYSTEM SlotWindow;
WINDOW_SYSTEM StageWindow = { StageMenu };
WINDOW_SYSTEM PracticeWindow = { PracticeMenu };
WINDOW_SYSTEM PromptWindow = { PromptMenu };
WINDOW_SYSTEM OverwriteWindow = { OverwriteMenu };
WINDOW_SYSTEM MessageWindow = { MessageMenu };

size_t SlotListSize(void)
{
	return REPLAY_SLOT_COUNT;
}

const char* SlotStateLabel(REPLAY_SLOT_STATE state)
{
	switch(state) {
		case REPLAY_SLOT_STATE::EMPTY:        return "<Empty>";
		case REPLAY_SLOT_STATE::CORRUPT:      return "<Corrupt>";
		case REPLAY_SLOT_STATE::INCOMPATIBLE: return "<Incompatible>";
		default:                              return "";
	}
}

const char* ModeLabel(REPLAY_MODE mode)
{
	switch(mode) {
		case REPLAY_MODE::NORMAL:   return "Normal";
		case REPLAY_MODE::EXTRA:    return "Extra";
		case REPLAY_MODE::PRACTICE: return "Practice";
		case REPLAY_MODE::PARTIAL:  return "Partial";
	}
	return "Unknown";
}

const char* DifficultyLabel(uint8_t difficulty)
{
	static constexpr const char* LABEL[] = {
		"Easy", "Normal", "Hard", "Lunatic"
	};
	return (difficulty < std::size(LABEL)) ? LABEL[difficulty] : "Extra";
}

void SlotGenerate(
	WINDOW_CHOICE& ret, size_t generated, size_t selected
)
{
	const auto& info = Slots[generated];
	if(info.State == REPLAY_SLOT_STATE::VALID) {
		sprintf(
			SlotTitle[generated],
			" %02zu  %-8.8s  Stage %s",
			(generated + 1),
			info.Name.data(),
			(info.ReachedStage == GRAPH_ID_EXSTAGE) ? "EX" :
				std::to_string(info.ReachedStage).c_str()
		);
	} else {
		sprintf(
			SlotTitle[generated],
			" %02zu  %s",
			(generated + 1),
			SlotStateLabel(info.State)
		);
	}
	ret.Title = SlotTitle[generated];
	ret.Help = (
		(State == UI_STATE::SAVE_SLOTS)
			? "Select this slot for saving"
			: "Select a valid replay for playback"
	);
	ret.Flags = (
		(generated == selected) ? WINDOW_FLAGS::HIGHLIGHT : WINDOW_FLAGS::NONE
	);
	if(generated == selected) {
		HighlightedSlot = generated;
		sprintf(
			SlotHeader,
			" %s Replay (%02zu/20)",
			(State == UI_STATE::SAVE_SLOTS) ? "Save" : "Load",
			(generated + 1)
		);
	}
}

void OpenSlotList(UI_STATE state, size_t selected);

bool SlotHandle(INPUT_BITS key, size_t selected)
{
	if(!Input_IsOK(key)) {
		return true;
	}

	SelectedSlot = selected;
	const auto& info = Slots[selected];
	if(State == UI_STATE::LOAD_SLOTS) {
		if(info.State != REPLAY_SLOT_STATE::VALID) {
			Snd_SEPlay(SOUND_ID_CANCEL);
			return true;
		}

		for(size_t i = 0; i < info.StageCount; i++) {
			const auto stage = info.Stages[i];
			sprintf(
				StageTitle[i],
				" Stage %s",
				(stage == GRAPH_ID_EXSTAGE) ? "EX" :
					std::to_string(stage).c_str()
			);
			StageItem[i] = {
				StageTitle[i],
				"Start playback here and continue through later stages",
				[](INPUT_BITS stage_key) {
					if(Input_IsCancel(stage_key)) {
						OpenSlotList(UI_STATE::LOAD_SLOTS, SelectedSlot);
						return true;
					}
					if(Input_IsOK(stage_key)) {
						const auto index = StageWindow.Select[0];
						const auto stage = Slots[SelectedSlot].Stages[index];
						if(GameReplayInit(SelectedSlot, stage)) {
							State = UI_STATE::NONE;
						}
					}
					return true;
				}
			};
		}
		StageMenu.NumItems = info.StageCount;
		StageWindow.Init(230);
		StageWindow.OpenCentered(230, 0);
		State = UI_STATE::LOAD_STAGES;
		return true;
	}

	if(info.State == REPLAY_SLOT_STATE::VALID) {
		OverwriteWindow.Init(210);
		OverwriteWindow.OpenCentered(210, 1);
		State = UI_STATE::OVERWRITE;
	} else {
		ReplayName = ReplayDefaultName();
		for(size_t i = 0; i < REPLAY_NAME_LENGTH; i++) {
			if(ReplayName[i] == '\0') {
				ReplayName[i] = ' ';
			}
		}
		NameCursor = 0;
		NameOldKey = Key_Data;
		Message.clear();
		State = UI_STATE::NAME;
	}
	return true;
}

WINDOW_MENU_SCROLL<
	SlotTitleItem, SlotListSize, SlotGenerate, SlotHandle, SLOT_VISIBLE
> SlotMenu;
WINDOW_SYSTEM SlotWindow = { SlotMenu.Menu };

void OpenSlotList(UI_STATE state, size_t selected)
{
	State = state;
	Slots = ReplayScanSlots();
	SlotMenu.Init(
		SlotWindow,
		(std::min)(selected, (REPLAY_SLOT_COUNT - 1)),
		(state == UI_STATE::LOAD_SLOTS) ? &MainWindow : nullptr
	);
	SlotWindow.Init(330);
	SlotWindow.OpenCentered(330, SlotWindow.Select[0]);
}

void Complete(void)
{
	const auto done = Done;
	Done = nullptr;
	State = UI_STATE::NONE;
	if(done) {
		done();
	}
}

bool PromptYes(INPUT_BITS key)
{
	if(Input_IsOK(key)) {
		OpenSlotList(UI_STATE::SAVE_SLOTS, 0);
	} else if(Input_IsCancel(key)) {
		ReplayRecordDiscard();
		Complete();
	}
	return true;
}

bool PromptNo(INPUT_BITS key)
{
	if(Input_IsOK(key) || Input_IsCancel(key)) {
		ReplayRecordDiscard();
		Complete();
	}
	return true;
}

void OpenNameEntry(void)
{
	ReplayName = ReplayDefaultName();
	for(size_t i = 0; i < REPLAY_NAME_LENGTH; i++) {
		if(ReplayName[i] == '\0') {
			ReplayName[i] = ' ';
		}
	}
	NameCursor = 0;
	NameOldKey = Key_Data;
	Message.clear();
	State = UI_STATE::NAME;
}

bool OverwriteYes(INPUT_BITS key)
{
	if(Input_IsOK(key)) {
		OpenNameEntry();
	} else if(Input_IsCancel(key)) {
		OpenSlotList(UI_STATE::SAVE_SLOTS, SelectedSlot);
	}
	return true;
}

bool OverwriteNo(INPUT_BITS key)
{
	if(Input_IsOK(key) || Input_IsCancel(key)) {
		OpenSlotList(UI_STATE::SAVE_SLOTS, SelectedSlot);
	}
	return true;
}

bool MessageOK(INPUT_BITS key)
{
	if(Input_IsOK(key) || Input_IsCancel(key)) {
		if(Done) {
			Complete();
		} else {
			State = UI_STATE::NONE;
			MainWindow.OldKey = key;
		}
	}
	return true;
}

void NameMove(void)
{
	if(Key_Data == NameOldKey) {
		return;
	}
	NameOldKey = Key_Data;
	if(Key_Data == 0) {
		return;
	}

	static constexpr std::string_view CHARS = (
		" ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789!?._-"
	);
	auto char_index = CHARS.find(ReplayName[NameCursor]);
	if(char_index == std::string_view::npos) {
		char_index = 0;
	}

	if(Key_Data == KEY_UP) {
		char_index = ((char_index + CHARS.size() - 1) % CHARS.size());
		ReplayName[NameCursor] = CHARS[char_index];
		Snd_SEPlay(SOUND_ID_SELECT);
	} else if(Key_Data == KEY_DOWN) {
		char_index = ((char_index + 1) % CHARS.size());
		ReplayName[NameCursor] = CHARS[char_index];
		Snd_SEPlay(SOUND_ID_SELECT);
	} else if(Key_Data == KEY_LEFT) {
		NameCursor = ((NameCursor + REPLAY_NAME_LENGTH - 1) % REPLAY_NAME_LENGTH);
		Snd_SEPlay(SOUND_ID_SELECT);
	} else if(Key_Data == KEY_RIGHT) {
		NameCursor = ((NameCursor + 1) % REPLAY_NAME_LENGTH);
		Snd_SEPlay(SOUND_ID_SELECT);
	} else if((Key_Data == KEY_BOMB) || (Key_Data == KEY_ESC)) {
		Snd_SEPlay(SOUND_ID_CANCEL);
		OpenSlotList(UI_STATE::SAVE_SLOTS, SelectedSlot);
	} else if(Input_IsOK(Key_Data)) {
		if(NameCursor < (REPLAY_NAME_LENGTH - 1)) {
			NameCursor++;
			Snd_SEPlay(SOUND_ID_SELECT);
		} else {
			ReplayName[REPLAY_NAME_LENGTH] = '\0';
			if(ReplaySaveSlot(SelectedSlot, ReplayName)) {
				Snd_SEPlay(SOUND_ID_SELECT);
				Complete();
			} else {
				Message = ReplayLastError();
				Snd_SEPlay(SOUND_ID_CANCEL);
			}
		}
	}
}

void DrawSlotDetails(void)
{
	const auto slot = HighlightedSlot;
	const auto& info = Slots[slot];
	if(info.State != REPLAY_SLOT_STATE::VALID) {
		GrpPut16(144, 388, SlotStateLabel(info.State));
		return;
	}

	const auto day = std::chrono::floor<std::chrono::days>(
		std::chrono::sys_seconds(std::chrono::seconds(info.TimestampUTC))
	);
	const std::chrono::year_month_day ymd(day);
	char detail[160];
	sprintf(
		detail,
		"UTC %04d-%02u-%02u  Score %lld  Graze %u",
		static_cast<int>(ymd.year()),
		static_cast<unsigned>(ymd.month()),
		static_cast<unsigned>(ymd.day()),
		static_cast<long long>(info.Score),
		info.Graze
	);
	GrpPut16(144, 388, detail);
	sprintf(
		detail,
		"%s  Weapon %u  %s  Stages %s-%s",
		DifficultyLabel(info.Difficulty),
		static_cast<unsigned>(info.Weapon + 1),
		ModeLabel(info.Mode),
		(info.Stages[0] == GRAPH_ID_EXSTAGE) ? "EX" :
			std::to_string(info.Stages[0]).c_str(),
		(info.Stages[info.StageCount - 1] == GRAPH_ID_EXSTAGE) ? "EX" :
			std::to_string(info.Stages[info.StageCount - 1]).c_str()
	);
	GrpPut16(144, 408, detail);
}

} // namespace

void ReplayUIOpenLoad(void)
{
	Done = nullptr;
	OpenSlotList(UI_STATE::LOAD_SLOTS, 0);
}

void ReplayUIOpenPractice(void)
{
	Done = nullptr;
	for(uint8_t i = 0; i < STAGE_MAX; i++) {
		const auto stage = static_cast<uint8_t>(i + 1);
		sprintf(
			PracticeTitle[i], " Stage %u", static_cast<unsigned>(stage)
		);
		PracticeItem[i] = {
			PracticeTitle[i],
			"",
			[](INPUT_BITS key) {
				if(Input_IsCancel(key)) {
					State = UI_STATE::NONE;
					PracticeWindow.State = CWIN_DEAD;
					MainWindow.OldKey = key;
				} else if(Input_IsOK(key)) {
					const auto stage = static_cast<uint8_t>(
						PracticeWindow.Select[0] + 1
					);
					if(PracticeStartInit(stage)) {
						State = UI_STATE::NONE;
					}
				}
				return true;
			}
		};
	}
	const auto extra_index = static_cast<size_t>(STAGE_MAX);
	strcpy(PracticeTitle[extra_index], " Extra Stage");
	PracticeItem[extra_index] = {
		PracticeTitle[extra_index],
		"",
		[](INPUT_BITS key) {
			if(Input_IsCancel(key)) {
				State = UI_STATE::NONE;
				PracticeWindow.State = CWIN_DEAD;
				MainWindow.OldKey = key;
			} else if(Input_IsOK(key) && ConfigDat.ExtraStgFlags.v) {
				if(PracticeStartInit(GRAPH_ID_EXSTAGE)) {
					State = UI_STATE::NONE;
				}
			}
			return true;
		},
		ConfigDat.ExtraStgFlags.v
			? WINDOW_FLAGS::NONE
			: WINDOW_FLAGS::DISABLED
	};
	PracticeMenu.NumItems = (STAGE_MAX + 1);
	PracticeWindow.Init(230);
	PracticeWindow.OpenCentered(230, 0);
	State = UI_STATE::PRACTICE;
}

void ReplayUIOpenSave(void (*done)(void))
{
	Done = done;
	if(!ReplayRecordReady()) {
		Message = ReplayRecordOverflowed()
			? "Replay is unsavable: a stage exceeded 108000 frames."
			: "Replay is unsavable.";
		MessageWindow.Init(180);
		MessageWindow.OpenCentered(180, 0);
		State = UI_STATE::MESSAGE;
		return;
	}

	PromptWindow.Init(210);
	PromptWindow.OpenCentered(210, 0);
	State = UI_STATE::SAVE_PROMPT;
}

void ReplayUIShowMessage(const char* message)
{
	Done = nullptr;
	Message = message ? message : "Replay error.";
	MessageWindow.Init(180);
	MessageWindow.OpenCentered(180, 0);
	State = UI_STATE::MESSAGE;
}

bool ReplayUIActive(void)
{
	return (State != UI_STATE::NONE);
}

tagWINDOW_SYSTEM* ReplayUIWindow(void)
{
	switch(State) {
		case UI_STATE::LOAD_SLOTS:
		case UI_STATE::SAVE_SLOTS:  return &SlotWindow;
		case UI_STATE::LOAD_STAGES: return &StageWindow;
		case UI_STATE::PRACTICE:    return &PracticeWindow;
		case UI_STATE::SAVE_PROMPT: return &PromptWindow;
		case UI_STATE::OVERWRITE:   return &OverwriteWindow;
		case UI_STATE::MESSAGE:     return &MessageWindow;
		default:                    return nullptr;
	}
}

void ReplayUIMove(void)
{
	if(State == UI_STATE::NAME) {
		NameMove();
		return;
	}

	auto* window = ReplayUIWindow();
	if(!window) {
		return;
	}
	CWinMove(window);
	if(window->State == CWIN_DEAD) {
		if((State == UI_STATE::LOAD_SLOTS) || (State == UI_STATE::PRACTICE)) {
			State = UI_STATE::NONE;
		} else if(State == UI_STATE::SAVE_SLOTS) {
			PromptWindow.Init(210);
			PromptWindow.OpenCentered(210, 0);
			State = UI_STATE::SAVE_PROMPT;
		}
	}
}

void ReplayUIDraw(void)
{
	if(State == UI_STATE::NAME) {
		char name[REPLAY_NAME_LENGTH + 1];
		memcpy(name, ReplayName.data(), REPLAY_NAME_LENGTH);
		name[REPLAY_NAME_LENGTH] = '\0';
		GrpPut16(220, 170, "REPLAY NAME");
		GrpPut16(220, 210, name);
		char cursor[REPLAY_NAME_LENGTH + 1] = {};
		memset(cursor, ' ', REPLAY_NAME_LENGTH);
		cursor[NameCursor] = '^';
		GrpPut16(220, 228, cursor);
		GrpPut16(132, 270, "UP/DOWN: Character  LEFT/RIGHT: Position");
		GrpPut16(132, 290, "SHOT/ENTER: Next or Save  ESC/BOMB: Back");
		if(!Message.empty()) {
			GrpPut16(132, 330, Message.c_str());
		}
		return;
	}

	if(auto* window = ReplayUIWindow()) {
		CWinDraw(window);
	}
	if((State == UI_STATE::LOAD_SLOTS) || (State == UI_STATE::SAVE_SLOTS)) {
		DrawSlotDetails();
	} else if(State == UI_STATE::MESSAGE) {
		GrpPut16(132, 360, Message.c_str());
	}
}
