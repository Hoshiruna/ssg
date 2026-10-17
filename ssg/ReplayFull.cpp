/*
 *   Full replay implementation
 */

#include "ssg/Replay.hpp"
#include "ssg/Gian.h"
#include "ssg/SCL.h"
#include "ssg/internal/SSG.hpp"
#include "ssg/internal/LZ.hpp"

namespace Replay {
namespace {
constexpr uint32_t Magic = 0x32524753;
// SGR2
constexpr uint16_t FormatVersion = 1;
constexpr uint16_t LogicVersion = 1;
constexpr size_t FullHeaderSize = 29;

struct ByteWriter {
	BUFFER_GROWABLE Bytes;

	void PutLE(uint64_t value, size_t byte_count)
	{
		for(size_t i = 0; i < byte_count; i++) {
			Bytes.push_back(static_cast<uint8_t>(value >> (i * 8)));
		}
	}
};

struct ByteReader {
	BUFFER_BORROWED Bytes;
	size_t Offset = 0;
	bool Valid = true;

	uint64_t GetLE(size_t byte_count)
	{
		if(byte_count > Bytes.size() - Offset) {
			Valid = false;
			return 0;
		}
		uint64_t value = 0;
		for(size_t i = 0; i < byte_count; i++) {
			value |= uint64_t(Bytes[Offset++]) << (i * 8);
		}
		return value;
	}
};

const char* CheckpointMismatch(const Checkpoint& expected, const Checkpoint& actual)
{
	if(expected.StepCount != actual.StepCount) {
		return "checkpoint: StepCount differs";
	}
	if(expected.Score != actual.Score) {
		return "checkpoint: Score differs";
	}
	if(expected.Seed != actual.Seed) {
		return "checkpoint: Seed differs";
	}
	if(expected.Rank != actual.Rank) {
		return "checkpoint: Rank differs";
	}
	if(expected.Stage != actual.Stage) {
		return "checkpoint: Stage differs";
	}
	if(expected.Lives != actual.Lives) {
		return "checkpoint: Lives differ";
	}
	if(expected.Bombs != actual.Bombs) {
		return "checkpoint: Bombs differ";
	}
	if(expected.Power != actual.Power) {
		return "checkpoint: Power differs";
	}
	return nullptr;
}

bool StartParamsValid(const FullData& data)
{
	return (
		((data.GameMode == Mode::Main && data.FirstStage == 1) ||
		 (data.GameMode == Mode::Extra && data.FirstStage == STAGE_EXTRA) ||
		 (data.GameMode == Mode::Practice && data.FirstStage >= 1 && data.FirstStage <= STAGE_MAX)) &&
		data.Round.LevelSelected <= GAME_LUNATIC &&
		data.Round.Weapon < 3 &&
		data.Round.MsgEnabled <= 1
	);
}
} // namespace

bool FullIdentify(BUFFER_BORROWED bytes)
{
	const auto dat = FilStartR(bytes);
	if(!dat || dat.info.size() != 2 || dat.info[0].size_uncompressed != FullHeaderSize) {
		return false;
	}
	const auto header = dat.MemExpand(0);
	if(!header) {
		return false;
	}
	ByteReader reader{static_cast<BUFFER_BORROWED>(header)};
	return reader.GetLE(4) == Magic && reader.Valid;
}

BUFFER_GROWABLE FullEncode(const FullData& data)
{
	if(
		!StartParamsValid(data) ||
		data.Records.size() > (256u * 1024u * 1024u) / sizeof(Record)
	) {
		return {};
	}

	ByteWriter writer;
	writer.PutLE(Magic, 4);
	writer.PutLE(FormatVersion, 2);
	writer.PutLE(LogicVersion, 2);
	writer.PutLE(std::to_underlying(data.GameMode), 1);
	writer.PutLE(data.FirstStage, 1);
	writer.PutLE(data.Round.Seed, 4);
	for(auto value : {
		data.Round.LevelSelected,
		data.Round.PlayerStock,
		data.Round.BombStock,
		data.Round.InputFlags,
		data.Round.Weapon,
		data.Round.Exp,
		uint8_t(data.Round.MsgEnabled)
	}) {
		writer.PutLE(value, 1);
	}
	writer.PutLE(data.Records.size(), 8);

	for(const auto& record : data.Records) {
		writer.PutLE(std::to_underlying(record.Kind), 1);
		writer.PutLE(record.Value, 2);
		if(record.Kind == Operation::Checkpoint || record.Kind == Operation::End) {
			const auto& state = record.State;
			writer.PutLE(state.StepCount, 8);
			writer.PutLE(static_cast<uint64_t>(state.Score), 8);
			writer.PutLE(state.Seed, 4);
			writer.PutLE(static_cast<uint32_t>(state.Rank), 4);
			for(auto value : {state.Stage, state.Lives, state.Bombs, state.Power}) {
				writer.PutLE(value, 1);
			}
		}
	}
	if(writer.Bytes.size() > 256u * 1024u * 1024u) {
		return {};
	}
	const BUFFER_BORROWED payload{writer.Bytes};
	const BUFFER_BORROWED members[] = {
		payload.first(FullHeaderSize),
		payload.subspan(FullHeaderSize)
	};
	return FilEncode(members);
}

const char* FullDecode(BUFFER_BORROWED bytes, FullData& out)
{
	if(bytes.size() > 256u * 1024u * 1024u) {
		return "replay file exceeds size limit";
	}
	const auto dat = FilStartR(bytes);
	if(!dat) {
		return "invalid DAT header or checksum";
	}
	if(
		dat.info.size() != 2 ||
		dat.info[0].size_uncompressed != FullHeaderSize ||
		dat.info[1].size_uncompressed > 256u * 1024u * 1024u - FullHeaderSize
	) {
		return "invalid DAT replay members";
	}
	const auto header = dat.MemExpand(0);
	const auto record_buffer = dat.MemExpand(1);
	if(!header || !record_buffer) {
		return "invalid DAT compressed replay data";
	}
	BUFFER_GROWABLE expanded(header.get(), header.get() + header.size());
	expanded.insert(expanded.end(), record_buffer.get(), record_buffer.get() + record_buffer.size());
	bytes = BUFFER_BORROWED{expanded};
	ByteReader reader{bytes};
	if(reader.GetLE(4) != Magic) {
		return "invalid replay signature";
	}
	if(reader.GetLE(2) != FormatVersion) {
		return "unsupported replay format version";
	}
	if(reader.GetLE(2) != LogicVersion) {
		return "incompatible replay logic version";
	}
	FullData data;
	data.GameMode = static_cast<Mode>(reader.GetLE(1));
	data.FirstStage = static_cast<uint8_t>(reader.GetLE(1));
	data.Round.Seed = static_cast<uint32_t>(reader.GetLE(4));
	for(auto field : {
		&data.Round.LevelSelected,
		&data.Round.PlayerStock,
		&data.Round.BombStock,
		&data.Round.InputFlags,
		&data.Round.Weapon,
		&data.Round.Exp,
		&data.Round.MsgEnabled
	}) {
		*field = static_cast<uint8_t>(reader.GetLE(1));
	}

	const auto record_count = reader.GetLE(8);
	if(
		!reader.Valid || !StartParamsValid(data) ||
		record_count > (bytes.size() - reader.Offset) / 3 ||
		record_count > (256u * 1024u * 1024u) / sizeof(Record)
	) {
		return "invalid replay header or record count";
	}

	for(uint64_t i = 0; i < record_count; i++) {
		Record record;
		record.Kind = static_cast<Operation>(reader.GetLE(1));
		record.Value = static_cast<uint16_t>(reader.GetLE(2));
		if(record.Kind > Operation::End) {
			return "unknown replay operation";
		}
		if(record.Kind == Operation::Checkpoint || record.Kind == Operation::End) {
			auto& state = record.State;
			state.StepCount = reader.GetLE(8);
			state.Score = std::bit_cast<int64_t>(reader.GetLE(8));
			state.Seed = static_cast<uint32_t>(reader.GetLE(4));
			state.Rank = std::bit_cast<int32_t>(static_cast<uint32_t>(reader.GetLE(4)));
			for(auto field : {&state.Stage, &state.Lives, &state.Bombs, &state.Power}) {
				*field = static_cast<uint8_t>(reader.GetLE(1));
			}
		}
		if(!reader.Valid) {
			return "truncated replay record";
		}
		if((record.Kind == Operation::End) != (i + 1 == record_count)) {
			return "replay must end with one End record";
		}
		data.Records.push_back(record);
	}
	if(record_count == 0 || reader.Offset != bytes.size()) {
		return "missing end or trailing replay data";
	}

	out = std::move(data);
	return nullptr;
}

Checkpoint FullPlayer::CaptureCheckpoint() const
{
	return {
		StepCount,
		Game.Viv.score,
		Game.RNG.seed,
		Game.PlayRank.Rank(),
		Game.Stage.Number(),
		Game.Viv.left,
		Game.Viv.bomb,
		Game.Viv.exp
	};
}

const char* FullPlayer::Start(const FullData& data)
{
	PlaybackEnded = true;
	if(!StartParamsValid(data)) {
		return "invalid starting parameters";
	}
	GameMode = data.GameMode;
	StepCount = 0;
	CheckpointPending = Cleared = GameOverActive = false;
	Game.RoundInit(data.Round, data.FirstStage);
	if(!Game.StageLoadFromDAT(Resources, data.FirstStage)) {
		return "stage load failed";
	}
	PlaybackEnded = false;
	return nullptr;
}

const char* FullPlayer::ApplyRecord(const Record& record)
{
	if(PlaybackEnded) {
		return "operation after replay end";
	}
	if(CheckpointPending && record.Kind != Operation::Checkpoint) {
		return "missing stage checkpoint";
	}

	switch(record.Kind) {
	case Operation::Frame:
		if(
			Cleared || GameOverActive || Game.Viv.GameOverTimer ||
			(record.Value & ~INPUT_BITS(0x2ff))
		) {
			return "invalid gameplay frame";
		}
		Game.Move(record.Value);
		StepCount++;
		GameOverActive = Game.Viv.GameOverTimer > 0;
		CheckpointPending = Game.Stage.GetEndCommand() != 0;
		return nullptr;

	case Operation::GameOverFrame:
		if(record.Value || Cleared || Game.Viv.GameOverTimer <= 0) {
			return "invalid Game Over frame";
		}
		Game.MoveGameOver();
		StepCount++;
		return nullptr;

	case Operation::Continue:
		if(
			record.Value || Cleared || Game.Viv.GameOverTimer != 0 ||
			!GameOverActive || !Game.Viv.credit
		) {
			return "invalid continue";
		}
		Game.RoundContinue();
		GameOverActive = false;
		return nullptr;

	case Operation::Checkpoint:
		if(!CheckpointPending || record.Value) {
			return "unexpected checkpoint";
		}
		if(const auto error = CheckpointMismatch(record.State, CaptureCheckpoint())) {
			return error;
		}
		CheckpointPending = false;
		if(
			GameMode == Mode::Main &&
			Game.Stage.GetEndCommand() == SCL_STAGECLEAR &&
			Game.Stage.Number() < STAGE_MAX
		) {
			if(!Game.StageLoadFromDAT(Resources, Game.Stage.Number() + 1)) {
				PlaybackEnded = true;
				return "stage load failed";
			}
		} else {
			Cleared = true;
		}
		return nullptr;

	case Operation::End:
		if(record.Value > std::to_underlying(EndReason::Quit)) {
			return "invalid end reason";
		}
		if(record.Value == std::to_underlying(EndReason::Clear) && !Cleared) {
			return "clear before stage completion";
		}
		if(
			record.Value == std::to_underlying(EndReason::GameOver) &&
			(!GameOverActive || Game.Viv.GameOverTimer != 0)
		) {
			return "premature Game Over end";
		}
		if(const auto error = CheckpointMismatch(record.State, CaptureCheckpoint())) {
			return error;
		}
		PlaybackEnded = true;
		return nullptr;
	}
	return "unknown replay operation";
}

} // namespace Replay
