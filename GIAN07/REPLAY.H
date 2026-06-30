/*
 *   Versioned, multi-stage replay recording and playback
 */

#pragma once

#include "game/input.h"

constexpr size_t REPLAY_SLOT_COUNT = 20;
constexpr size_t REPLAY_NAME_LENGTH = 8;
constexpr size_t REPLAY_STAGE_COUNT_MAX = 7;
constexpr size_t REPLAY_STAGE_FRAME_MAX = (60 * 60 * 30);

enum class REPLAY_MODE : uint8_t {
	NORMAL,
	EXTRA,
	PRACTICE,
	PARTIAL,
};

enum class REPLAY_FINISH : uint8_t {
	CLEAR,
	QUIT,
	GAME_OVER,
	CONTINUE,
};

enum class REPLAY_SLOT_STATE : uint8_t {
	EMPTY,
	VALID,
	CORRUPT,
	INCOMPATIBLE,
};

struct REPLAY_SLOT_INFO {
	REPLAY_SLOT_STATE State = REPLAY_SLOT_STATE::EMPTY;
	std::array<char, (REPLAY_NAME_LENGTH + 1)> Name = {};
	uint64_t TimestampUTC = 0;
	int64_t Score = 0;
	uint32_t Graze = 0;
	uint8_t Difficulty = 0;
	uint8_t Weapon = 0;
	REPLAY_MODE Mode = REPLAY_MODE::NORMAL;
	uint8_t ReachedStage = 0;
	std::array<uint8_t, REPLAY_STAGE_COUNT_MAX> Stages = {};
	uint8_t StageCount = 0;
};

void ReplayRecordBegin(REPLAY_MODE mode);
void ReplayRecordStageBegin(uint8_t stage);
void ReplayRecordStageTransition(uint8_t stage);
void ReplayRecordFrame(INPUT_BITS input);
void ReplayRecordFinish(REPLAY_FINISH finish);
void ReplayRecordDiscard(void);
bool ReplayRecordActive(void);
bool ReplayRecordReady(void);
bool ReplayRecordOverflowed(void);
REPLAY_MODE ReplayRecordMode(void);

std::array<char, (REPLAY_NAME_LENGTH + 1)> ReplayDefaultName(void);
bool ReplaySaveSlot(
	size_t slot, const std::array<char, (REPLAY_NAME_LENGTH + 1)>& name
);

REPLAY_SLOT_INFO ReplayScanSlot(size_t slot);
std::array<REPLAY_SLOT_INFO, REPLAY_SLOT_COUNT> ReplayScanSlots(void);

bool ReplayPlaybackBegin(size_t slot, uint8_t stage);
void ReplayPlaybackApplyStart(void);
INPUT_BITS ReplayPlaybackInput(void);
bool ReplayPlaybackInputExhausted(void);
bool ReplayPlaybackStageBegin(uint8_t stage);
bool ReplayPlaybackFinish(void);
void ReplayPlaybackAbort(void);
bool ReplayPlaybackActive(void);
bool ReplayPlaybackHasStage(uint8_t stage);
uint8_t ReplayPlaybackFirstStage(void);
uint8_t ReplayPlaybackLastStage(void);
const char* ReplayLastError(void);
