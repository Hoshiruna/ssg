/*
 *   Full replay declarations
 */

#pragma once

#include "ssg/Replay.h"
#include "ssg/Round.h"

struct C_SSG;
struct PACKFILE_READ;

namespace Replay {
enum class Mode : uint8_t { Main, Extra, Practice };
enum class Operation : uint8_t { Frame, GameOverFrame, Continue, Checkpoint, End };
enum class EndReason : uint8_t { Clear, GameOver, Quit };
struct Checkpoint {
	uint64_t StepCount = 0;
	int64_t Score = 0;
	uint32_t Seed = 0;
	int32_t Rank = 0;
	uint8_t Stage = 0, Lives = 0, Bombs = 0, Power = 0;
	bool operator==(const Checkpoint&) const = default;
};
struct Record {
	Operation Kind = Operation::Frame;
	uint16_t Value = 0;
	Checkpoint State;
};
struct FullData {
	Mode GameMode = Mode::Main;
	uint8_t FirstStage = 1;
	ROUND_PARAMS Round = {};
	std::vector<Record> Records;
};
// TODO: This section requires modifying the reader to support it
// Returns an empty buffer if the starting parameters or size are invalid.
HATOYAMA_API BUFFER_GROWABLE FullEncode(const FullData& data);
// Returns nullptr on success or a static error message on failure.
// Replaces out only on success. The decoded data owns its storage.
HATOYAMA_API const char* FullDecode(BUFFER_BORROWED bytes, FullData& out);
// Checks the DAT checksum and replay signature, not the full operation stream.
HATOYAMA_API bool FullIdentify(BUFFER_BORROWED bytes);

// The game, resource pack, and its backing buffer must outlive this object.
// Frontend hooks must not change stages; ApplyRecord handles stage transitions.
class HATOYAMA_API FullPlayer {
	C_SSG& Game;
	const PACKFILE_READ& Resources;
	Mode GameMode = Mode::Main;
	uint64_t StepCount = 0;
	bool CheckpointPending = false;
	bool Cleared = false;
	bool PlaybackEnded = true;
	bool GameOverActive = false;
public:
	FullPlayer(C_SSG& game, const PACKFILE_READ& resources) :
		Game(game), Resources(resources) {
	}
	// Initializes a round from data; does not consume data.Records.
	// Start and ApplyRecord return nullptr on success or a static error message.
	const char* Start(const FullData& data);
	// Stop applying records after an error. Call Start to begin another round.
	// When NeedsCheckpoint() is true, the next record must be a Checkpoint.
	// Record CaptureCheckpoint() before applying the checkpoint.
	// Applying it may load the next stage.
	const char* ApplyRecord(const Record& record);
	Checkpoint CaptureCheckpoint() const;
	bool IsFinished() const { return PlaybackEnded; }
	bool NeedsCheckpoint() const { return CheckpointPending; }
};
} // namespace Replay
