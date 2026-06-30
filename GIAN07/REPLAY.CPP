/*
 *   Versioned, multi-stage replay recording and playback
 */

#include <SDL3/SDL_filesystem.h>

#include "REPLAY.H"
#include "CONFIG.H"
#include "GIAN.H"
#include "LOADER.H"
#include "LZ_UTY.H"
#include "MAID.H"
#include "PRankCtrl.h"
#include "game/ut_math.h"
#include "platform/path.h"

namespace {

constexpr std::array<uint8_t, 4> REPLAY_MAGIC = { 'S', 'S', 'G', 'R' };
constexpr uint16_t REPLAY_FORMAT_VERSION = 1;
constexpr uint16_t REPLAY_COMPATIBILITY_VERSION = 1;

struct REPLAY_SNAPSHOT {
	uint32_t RandomSeed = 0;
	int32_t PlayerX = 0;
	int32_t PlayerY = 0;
	int32_t PlayerVelocityX = 0;
	int32_t PlayerVelocityY = 0;
	int32_t OptionX = 0;
	int32_t OptionY = 0;
	int64_t Score = 0;
	int64_t ScoreDelta = 0;
	uint32_t GrazeTotal = 0;
	int32_t GrazeScore = 0;
	uint16_t Graze = 0;
	uint16_t GrazeChain = 0;
	uint16_t PowerProgress = 0;
	uint16_t BombTime = 0;
	uint16_t InvulnerabilityTime = 0;
	uint16_t LaserTime = 0;
	int8_t MoveSpeed = 0;
	uint8_t Weapon = 0;
	uint8_t Power = 0;
	uint8_t Bombs = 0;
	uint8_t Lives = 0;
	uint8_t Credits = 0;
	uint8_t GraphicsID = 0;
	uint8_t LaserGraphics = 0;
	uint8_t ShotTime = 0;
	uint8_t ShotExtra = 0;
	uint8_t SlowMoveCounter = 0;
	uint8_t GameOver = 0;
	uint8_t GrazeSound = 0;
	uint8_t Difficulty = 0;
	uint8_t InputFlags = 0;
	uint8_t StartingBombs = 0;
	uint8_t StartingLives = 0;
	uint8_t GraphicsFlags = 0;
	uint8_t RankDifficulty = 0;
	int32_t Rank = 0;
	uint32_t GameFrame = 0;
	uint8_t Stage = 0;
};

struct REPLAY_STAGE {
	uint8_t Stage = 0;
	REPLAY_SNAPSHOT Snapshot;
	uint32_t StartHash = 0;
	uint32_t EndHash = 0;
	std::vector<INPUT_BITS> Input;
};

struct REPLAY_RUN {
	std::array<char, (REPLAY_NAME_LENGTH + 1)> Name = {};
	uint64_t TimestampUTC = 0;
	int64_t FinalScore = 0;
	uint32_t FinalGraze = 0;
	uint8_t Difficulty = 0;
	uint8_t Weapon = 0;
	REPLAY_MODE Mode = REPLAY_MODE::NORMAL;
	REPLAY_FINISH Finish = REPLAY_FINISH::CLEAR;
	uint8_t ReachedStage = 0;
	std::vector<REPLAY_STAGE> Stages;
	bool Overflowed = false;
};

REPLAY_RUN Recorded;
bool Recording = false;
bool RecordReady = false;

REPLAY_RUN Playing;
bool Playback = false;
size_t PlaybackStageIndex = 0;
size_t PlaybackFrame = 0;
std::string LastError;
struct {
	uint8_t Difficulty = 0;
	uint8_t PlayerStock = 0;
	uint8_t BombStock = 0;
	uint8_t InputFlags = 0;
	uint8_t GraphicsFlags = 0;
	bool Valid = false;
} PlaybackConfig;

template <typename T> void PutLE(BYTE_BUFFER_GROWABLE& out, T value)
{
	using U = std::make_unsigned_t<T>;
	const U unsigned_value = static_cast<U>(value);
	for(size_t i = 0; i < sizeof(T); i++) {
		out.push_back(static_cast<uint8_t>(unsigned_value >> (i * 8)));
	}
}

void PutSnapshot(BYTE_BUFFER_GROWABLE& out, const REPLAY_SNAPSHOT& snapshot)
{
	PutLE(out, snapshot.RandomSeed);
	PutLE(out, snapshot.PlayerX);
	PutLE(out, snapshot.PlayerY);
	PutLE(out, snapshot.PlayerVelocityX);
	PutLE(out, snapshot.PlayerVelocityY);
	PutLE(out, snapshot.OptionX);
	PutLE(out, snapshot.OptionY);
	PutLE(out, snapshot.Score);
	PutLE(out, snapshot.ScoreDelta);
	PutLE(out, snapshot.GrazeTotal);
	PutLE(out, snapshot.GrazeScore);
	PutLE(out, snapshot.Graze);
	PutLE(out, snapshot.GrazeChain);
	PutLE(out, snapshot.PowerProgress);
	PutLE(out, snapshot.BombTime);
	PutLE(out, snapshot.InvulnerabilityTime);
	PutLE(out, snapshot.LaserTime);
	PutLE(out, snapshot.MoveSpeed);
	PutLE(out, snapshot.Weapon);
	PutLE(out, snapshot.Power);
	PutLE(out, snapshot.Bombs);
	PutLE(out, snapshot.Lives);
	PutLE(out, snapshot.Credits);
	PutLE(out, snapshot.GraphicsID);
	PutLE(out, snapshot.LaserGraphics);
	PutLE(out, snapshot.ShotTime);
	PutLE(out, snapshot.ShotExtra);
	PutLE(out, snapshot.SlowMoveCounter);
	PutLE(out, snapshot.GameOver);
	PutLE(out, snapshot.GrazeSound);
	PutLE(out, snapshot.Difficulty);
	PutLE(out, snapshot.InputFlags);
	PutLE(out, snapshot.StartingBombs);
	PutLE(out, snapshot.StartingLives);
	PutLE(out, snapshot.GraphicsFlags);
	PutLE(out, snapshot.RankDifficulty);
	PutLE(out, snapshot.Rank);
	PutLE(out, snapshot.GameFrame);
	PutLE(out, snapshot.Stage);
}

class REPLAY_READER {
	std::span<const uint8_t> Data;
	size_t Cursor = 0;

public:
	explicit REPLAY_READER(std::span<const uint8_t> data) : Data(data) {
	}

	template <typename T> bool Get(T& value)
	{
		if((Data.size() - (std::min)(Data.size(), Cursor)) < sizeof(T)) {
			return false;
		}

		using U = std::make_unsigned_t<T>;
		U result = 0;
		for(size_t i = 0; i < sizeof(T); i++) {
			result |= (static_cast<U>(Data[Cursor++]) << (i * 8));
		}
		value = static_cast<T>(result);
		return true;
	}

	bool GetBytes(void* destination, size_t size)
	{
		if((Data.size() - (std::min)(Data.size(), Cursor)) < size) {
			return false;
		}
		memcpy(destination, &Data[Cursor], size);
		Cursor += size;
		return true;
	}

	bool AtEnd(void) const {
		return (Cursor == Data.size());
	}
};

bool GetSnapshot(REPLAY_READER& in, REPLAY_SNAPSHOT& snapshot)
{
	return (
		in.Get(snapshot.RandomSeed) &&
		in.Get(snapshot.PlayerX) &&
		in.Get(snapshot.PlayerY) &&
		in.Get(snapshot.PlayerVelocityX) &&
		in.Get(snapshot.PlayerVelocityY) &&
		in.Get(snapshot.OptionX) &&
		in.Get(snapshot.OptionY) &&
		in.Get(snapshot.Score) &&
		in.Get(snapshot.ScoreDelta) &&
		in.Get(snapshot.GrazeTotal) &&
		in.Get(snapshot.GrazeScore) &&
		in.Get(snapshot.Graze) &&
		in.Get(snapshot.GrazeChain) &&
		in.Get(snapshot.PowerProgress) &&
		in.Get(snapshot.BombTime) &&
		in.Get(snapshot.InvulnerabilityTime) &&
		in.Get(snapshot.LaserTime) &&
		in.Get(snapshot.MoveSpeed) &&
		in.Get(snapshot.Weapon) &&
		in.Get(snapshot.Power) &&
		in.Get(snapshot.Bombs) &&
		in.Get(snapshot.Lives) &&
		in.Get(snapshot.Credits) &&
		in.Get(snapshot.GraphicsID) &&
		in.Get(snapshot.LaserGraphics) &&
		in.Get(snapshot.ShotTime) &&
		in.Get(snapshot.ShotExtra) &&
		in.Get(snapshot.SlowMoveCounter) &&
		in.Get(snapshot.GameOver) &&
		in.Get(snapshot.GrazeSound) &&
		in.Get(snapshot.Difficulty) &&
		in.Get(snapshot.InputFlags) &&
		in.Get(snapshot.StartingBombs) &&
		in.Get(snapshot.StartingLives) &&
		in.Get(snapshot.GraphicsFlags) &&
		in.Get(snapshot.RankDifficulty) &&
		in.Get(snapshot.Rank) &&
		in.Get(snapshot.GameFrame) &&
		in.Get(snapshot.Stage)
	);
}

REPLAY_SNAPSHOT CaptureSnapshot(void)
{
	return {
		.RandomSeed = rnd_seed_get(),
		.PlayerX = Viv.x,
		.PlayerY = Viv.y,
		.PlayerVelocityX = Viv.vx,
		.PlayerVelocityY = Viv.vy,
		.OptionX = Viv.opx,
		.OptionY = Viv.opy,
		.Score = Viv.score,
		.ScoreDelta = Viv.dscore,
		.GrazeTotal = Viv.evade_sum,
		.GrazeScore = Viv.evadesc,
		.Graze = Viv.evade,
		.GrazeChain = Viv.evade_c,
		.PowerProgress = Viv.exp2,
		.BombTime = Viv.bomb_time,
		.InvulnerabilityTime = Viv.muteki,
		.LaserTime = Viv.lay_time,
		.MoveSpeed = Viv.v,
		.Weapon = Viv.weapon,
		.Power = Viv.exp,
		.Bombs = Viv.bomb,
		.Lives = Viv.left,
		.Credits = Viv.credit,
		.GraphicsID = Viv.GrpID,
		.LaserGraphics = Viv.lay_grp,
		.ShotTime = Viv.toge_time,
		.ShotExtra = Viv.toge_ex,
		.SlowMoveCounter = Viv.ShiftCounter,
		.GameOver = Viv.bGameOver,
		.GrazeSound = Viv.BuzzSound,
		.Difficulty = GameLevel,
		.InputFlags = ConfigDat.InputFlags.v,
		.StartingBombs = ConfigDat.BombStock.v,
		.StartingLives = ConfigDat.PlayerStock.v,
		.GraphicsFlags = ConfigDat.GraphFlags.v,
		.RankDifficulty = PlayRank.GameLevel,
		.Rank = PlayRank.Rank,
		.GameFrame = GameCount,
		.Stage = GameStage,
	};
}

void ApplySnapshot(const REPLAY_SNAPSHOT& snapshot)
{
	ConfigDat.InputFlags.v = snapshot.InputFlags;
	ConfigDat.BombStock.v = snapshot.StartingBombs;
	ConfigDat.PlayerStock.v = snapshot.StartingLives;
	ConfigDat.GameLevel.v = snapshot.Difficulty;
	ConfigDat.GraphFlags.v = snapshot.GraphicsFlags;

	GameLevel = snapshot.Difficulty;
	GameStage = snapshot.Stage;

	Viv.x = snapshot.PlayerX;
	Viv.y = snapshot.PlayerY;
	Viv.vx = snapshot.PlayerVelocityX;
	Viv.vy = snapshot.PlayerVelocityY;
	Viv.opx = snapshot.OptionX;
	Viv.opy = snapshot.OptionY;
	Viv.score = snapshot.Score;
	Viv.dscore = snapshot.ScoreDelta;
	Viv.evade_sum = snapshot.GrazeTotal;
	Viv.evadesc = snapshot.GrazeScore;
	Viv.evade = snapshot.Graze;
	Viv.evade_c = snapshot.GrazeChain;
	Viv.exp2 = snapshot.PowerProgress;
	Viv.bomb_time = snapshot.BombTime;
	Viv.muteki = snapshot.InvulnerabilityTime;
	Viv.lay_time = snapshot.LaserTime;
	Viv.v = snapshot.MoveSpeed;
	Viv.weapon = snapshot.Weapon;
	Viv.exp = snapshot.Power;
	Viv.bomb = snapshot.Bombs;
	Viv.left = snapshot.Lives;
	Viv.credit = snapshot.Credits;
	Viv.GrpID = snapshot.GraphicsID;
	Viv.lay_grp = snapshot.LaserGraphics;
	Viv.toge_time = snapshot.ShotTime;
	Viv.toge_ex = snapshot.ShotExtra;
	Viv.ShiftCounter = snapshot.SlowMoveCounter;
	Viv.bGameOver = (snapshot.GameOver != 0);
	Viv.BuzzSound = (snapshot.GrazeSound != 0);

	PlayRank.GameLevel = snapshot.RankDifficulty;
	PlayRank.Rank = snapshot.Rank;
	GameCount = snapshot.GameFrame;
	rnd_seed_set(snapshot.RandomSeed);
}

uint32_t SnapshotHash(const REPLAY_SNAPSHOT& snapshot)
{
	BYTE_BUFFER_GROWABLE bytes;
	PutSnapshot(bytes, snapshot);

	uint32_t hash = 2166136261u;
	for(const auto byte : bytes) {
		hash ^= byte;
		hash *= 16777619u;
	}
	return hash;
}

uint32_t CurrentStateHash(void)
{
	return SnapshotHash(CaptureSnapshot());
}

std::u8string ReplayFilename(size_t slot, bool temporary = false)
{
	std::u8string ret = u8"replay/ssg_00.rep";
	const auto number = (slot + 1);
	ret[11] = static_cast<char8_t>('0' + (number / 10));
	ret[12] = static_cast<char8_t>('0' + (number % 10));
	if(temporary) {
		ret += u8".tmp";
	}
	return ret;
}

void SetError(const char* error)
{
	LastError = error;
}

void RestorePlaybackConfig(void)
{
	if(!PlaybackConfig.Valid) {
		return;
	}
	ConfigDat.GameLevel.v = PlaybackConfig.Difficulty;
	ConfigDat.PlayerStock.v = PlaybackConfig.PlayerStock;
	ConfigDat.BombStock.v = PlaybackConfig.BombStock;
	ConfigDat.InputFlags.v = PlaybackConfig.InputFlags;
	ConfigDat.GraphFlags.v = PlaybackConfig.GraphicsFlags;
	PlaybackConfig.Valid = false;
}

BYTE_BUFFER_GROWABLE SerializeMetadata(const REPLAY_RUN& run)
{
	BYTE_BUFFER_GROWABLE out;
	out.insert(out.end(), REPLAY_MAGIC.begin(), REPLAY_MAGIC.end());
	PutLE(out, REPLAY_FORMAT_VERSION);
	PutLE(out, REPLAY_COMPATIBILITY_VERSION);
	out.insert(
		out.end(),
		reinterpret_cast<const uint8_t *>(run.Name.data()),
		reinterpret_cast<const uint8_t *>(run.Name.data() + REPLAY_NAME_LENGTH)
	);
	PutLE(out, run.TimestampUTC);
	PutLE(out, run.FinalScore);
	PutLE(out, run.FinalGraze);
	PutLE(out, run.Difficulty);
	PutLE(out, run.Weapon);
	PutLE(out, static_cast<uint8_t>(run.Mode));
	PutLE(out, static_cast<uint8_t>(run.Finish));
	PutLE(out, run.ReachedStage);
	PutLE(out, static_cast<uint8_t>(run.Stages.size()));
	PutLE<uint16_t>(out, 0);

	for(const auto& stage : run.Stages) {
		PutLE(out, stage.Stage);
		PutLE<uint8_t>(out, 0);
		PutLE<uint16_t>(out, 0);
		PutLE(out, static_cast<uint32_t>(stage.Input.size()));
		PutSnapshot(out, stage.Snapshot);
		PutLE(out, stage.StartHash);
		PutLE(out, stage.EndHash);
	}
	return out;
}

enum class LOAD_RESULT {
	OK,
	CORRUPT,
	INCOMPATIBLE,
};

LOAD_RESULT ParseMetadata(std::span<const uint8_t> bytes, REPLAY_RUN& run)
{
	REPLAY_READER in(bytes);
	std::array<uint8_t, REPLAY_MAGIC.size()> magic;
	uint16_t format = 0;
	uint16_t compatibility = 0;
	uint8_t mode = 0;
	uint8_t finish = 0;
	uint8_t stage_count = 0;
	uint16_t reserved = 0;

	if(
		!in.GetBytes(magic.data(), magic.size()) ||
		!in.Get(format) ||
		!in.Get(compatibility)
	) {
		return LOAD_RESULT::CORRUPT;
	}
	if(magic != REPLAY_MAGIC) {
		return LOAD_RESULT::CORRUPT;
	}
	if(
		(format != REPLAY_FORMAT_VERSION) ||
		(compatibility != REPLAY_COMPATIBILITY_VERSION)
	) {
		return LOAD_RESULT::INCOMPATIBLE;
	}

	run = {};
	if(
		!in.GetBytes(run.Name.data(), REPLAY_NAME_LENGTH) ||
		!in.Get(run.TimestampUTC) ||
		!in.Get(run.FinalScore) ||
		!in.Get(run.FinalGraze) ||
		!in.Get(run.Difficulty) ||
		!in.Get(run.Weapon) ||
		!in.Get(mode) ||
		!in.Get(finish) ||
		!in.Get(run.ReachedStage) ||
		!in.Get(stage_count) ||
		!in.Get(reserved)
	) {
		return LOAD_RESULT::CORRUPT;
	}
	run.Name[REPLAY_NAME_LENGTH] = '\0';
	const bool name_valid = std::ranges::all_of(
		std::span(run.Name).first(REPLAY_NAME_LENGTH),
		[](unsigned char c) {
			return ((c == '\0') || ((c >= 0x20) && (c <= 0x7E)));
		}
	);
	if(
		!name_valid ||
		(run.TimestampUTC > 253402300799ULL) ||
		(run.FinalScore < 0) ||
		(run.Difficulty > GAME_LUNATIC) ||
		(run.Weapon > 2) ||
		(mode > static_cast<uint8_t>(REPLAY_MODE::PARTIAL)) ||
		(finish > static_cast<uint8_t>(REPLAY_FINISH::CONTINUE)) ||
		(stage_count == 0) ||
		(stage_count > REPLAY_STAGE_COUNT_MAX) ||
		(reserved != 0)
	) {
		return LOAD_RESULT::CORRUPT;
	}
	run.Mode = static_cast<REPLAY_MODE>(mode);
	run.Finish = static_cast<REPLAY_FINISH>(finish);
	run.Stages.reserve(stage_count);

	for(uint8_t i = 0; i < stage_count; i++) {
		REPLAY_STAGE stage;
		uint8_t padding8 = 0;
		uint16_t padding16 = 0;
		uint32_t frame_count = 0;
		if(
			!in.Get(stage.Stage) ||
			!in.Get(padding8) ||
			!in.Get(padding16) ||
			!in.Get(frame_count) ||
			!GetSnapshot(in, stage.Snapshot) ||
			!in.Get(stage.StartHash) ||
			!in.Get(stage.EndHash)
		) {
			return LOAD_RESULT::CORRUPT;
		}
		if(
			(stage.Stage != stage.Snapshot.Stage) ||
			(stage.Snapshot.Weapon != run.Weapon) ||
			(stage.Snapshot.Difficulty != run.Difficulty) ||
			(stage.Snapshot.RankDifficulty > GAME_LUNATIC) ||
			(stage.Snapshot.GameOver > 1) ||
			(stage.Snapshot.GrazeSound > 1) ||
			(padding8 != 0) ||
			(padding16 != 0) ||
			(
				(stage.Stage != GRAPH_ID_EXSTAGE) &&
				((stage.Stage == 0) || (stage.Stage > STAGE_MAX))
			) ||
			(frame_count > REPLAY_STAGE_FRAME_MAX) ||
			(stage.StartHash != SnapshotHash(stage.Snapshot))
		) {
			return LOAD_RESULT::CORRUPT;
		}
		if(
			!run.Stages.empty() &&
			(stage.Stage != static_cast<uint8_t>(run.Stages.back().Stage + 1))
		) {
			return LOAD_RESULT::CORRUPT;
		}
		stage.Input.resize(frame_count);
		if(std::ranges::any_of(run.Stages, [&stage](const auto& previous) {
			return (previous.Stage == stage.Stage);
		})) {
			return LOAD_RESULT::CORRUPT;
		}
		run.Stages.push_back(std::move(stage));
	}

	if(
		!in.AtEnd() ||
		(run.ReachedStage != run.Stages.back().Stage) ||
		(
			((run.Mode == REPLAY_MODE::EXTRA) ||
			 (run.Mode == REPLAY_MODE::PRACTICE)) &&
			(run.Stages.size() != 1)
		)
	) {
		return LOAD_RESULT::CORRUPT;
	}
	return LOAD_RESULT::OK;
}

LOAD_RESULT LoadReplayFile(
	const std::u8string& filename, REPLAY_RUN& run, bool metadata_only
)
{
	const auto in = FilStartR(filename.c_str());
	if(!in) {
		return LOAD_RESULT::CORRUPT;
	}

	const auto metadata = in.MemExpand(0);
	if(!metadata) {
		return LOAD_RESULT::CORRUPT;
	}
	const auto result = ParseMetadata({ metadata.get(), metadata.size() }, run);
	if(result != LOAD_RESULT::OK) {
		return result;
	}
	if(in.info.size() != (run.Stages.size() + 1)) {
		return LOAD_RESULT::CORRUPT;
	}
	for(size_t i = 0; i < run.Stages.size(); i++) {
		if(
			static_cast<size_t>(
				static_cast<fil_size_t>(in.info[i + 1].size_uncompressed)
			) !=
			(run.Stages[i].Input.size() * sizeof(uint16_t))
		) {
			return LOAD_RESULT::CORRUPT;
		}
	}
	if(metadata_only) {
		return LOAD_RESULT::OK;
	}

	for(size_t i = 0; i < run.Stages.size(); i++) {
		auto& destination = run.Stages[i].Input;
		if(destination.empty()) {
			continue;
		}
		const auto input = in.MemExpand(static_cast<fil_no_t>(i + 1));
		if(!input || (input.size() != (destination.size() * sizeof(uint16_t)))) {
			return LOAD_RESULT::CORRUPT;
		}
		REPLAY_READER input_reader({ input.get(), input.size() });
		for(auto& frame : destination) {
			if(!input_reader.Get(frame)) {
				return LOAD_RESULT::CORRUPT;
			}
		}
		if(!input_reader.AtEnd()) {
			return LOAD_RESULT::CORRUPT;
		}
	}
	return LOAD_RESULT::OK;
}

REPLAY_SLOT_INFO SlotInfo(const REPLAY_RUN& run)
{
	REPLAY_SLOT_INFO ret = {
		.State = REPLAY_SLOT_STATE::VALID,
		.Name = run.Name,
		.TimestampUTC = run.TimestampUTC,
		.Score = run.FinalScore,
		.Graze = run.FinalGraze,
		.Difficulty = run.Difficulty,
		.Weapon = run.Weapon,
		.Mode = run.Mode,
		.ReachedStage = run.ReachedStage,
		.StageCount = static_cast<uint8_t>(run.Stages.size()),
	};
	for(size_t i = 0; i < run.Stages.size(); i++) {
		ret.Stages[i] = run.Stages[i].Stage;
	}
	return ret;
}

} // namespace

void ReplayRecordBegin(REPLAY_MODE mode)
{
	Recorded = {};
	Recorded.Mode = mode;
	Recorded.Difficulty = GameLevel;
	Recorded.Weapon = Viv.weapon;
	Recording = true;
	RecordReady = false;
	LastError.clear();
}

void ReplayRecordStageBegin(uint8_t stage)
{
	if(!Recording || Recorded.Overflowed) {
		return;
	}
	if(Recorded.Stages.size() >= REPLAY_STAGE_COUNT_MAX) {
		Recorded.Overflowed = true;
		Recording = false;
		SetError("Replay contains too many stages.");
		return;
	}

	auto snapshot = CaptureSnapshot();
	snapshot.Stage = stage;
	Recorded.Stages.push_back({
		.Stage = stage,
		.Snapshot = snapshot,
		.StartHash = SnapshotHash(snapshot),
	});
	Recorded.ReachedStage = stage;
}

void ReplayRecordStageTransition(uint8_t stage)
{
	if(!Recording || Recorded.Overflowed || Recorded.Stages.empty()) {
		return;
	}
	Recorded.Stages.back().EndHash = CurrentStateHash();
	ReplayRecordStageBegin(stage);
}

void ReplayRecordFrame(INPUT_BITS input)
{
	if(!Recording || Recorded.Overflowed) {
		return;
	}
	if(Recorded.Stages.empty()) {
		return;
	}

	auto& frames = Recorded.Stages.back().Input;
	if(frames.size() >= REPLAY_STAGE_FRAME_MAX) {
		Recorded.Overflowed = true;
		Recording = false;
		RecordReady = false;
		SetError("Replay stage exceeded 108000 frames.");
		return;
	}
	frames.push_back(input);
}

void ReplayRecordFinish(REPLAY_FINISH finish)
{
	if(!Recording) {
		return;
	}
	if(!Recorded.Stages.empty()) {
		Recorded.Stages.back().EndHash = CurrentStateHash();
	}
	Recorded.Finish = finish;
	if(finish != REPLAY_FINISH::CLEAR) {
		Recorded.Mode = REPLAY_MODE::PARTIAL;
	}
	Recorded.FinalScore = Viv.score;
	Recorded.FinalGraze = Viv.evade_sum;
	Recorded.ReachedStage = GameStage;
	Recording = false;
	RecordReady = (!Recorded.Overflowed && !Recorded.Stages.empty());
}

void ReplayRecordDiscard(void)
{
	Recorded = {};
	Recording = false;
	RecordReady = false;
}

bool ReplayRecordActive(void)
{
	return Recording;
}

bool ReplayRecordReady(void)
{
	return RecordReady;
}

bool ReplayRecordOverflowed(void)
{
	return Recorded.Overflowed;
}

REPLAY_MODE ReplayRecordMode(void)
{
	return Recorded.Mode;
}

std::array<char, (REPLAY_NAME_LENGTH + 1)> ReplayDefaultName(void)
{
	std::array<char, (REPLAY_NAME_LENGTH + 1)> ret = {
		'V', 'i', 'v', 'i', 't', '!', '\0', '\0', '\0'
	};
	uint64_t newest = 0;
	for(size_t slot = 0; slot < REPLAY_SLOT_COUNT; slot++) {
		const auto info = ReplayScanSlot(slot);
		if(
			(info.State == REPLAY_SLOT_STATE::VALID) &&
			(info.TimestampUTC >= newest)
		) {
			newest = info.TimestampUTC;
			ret = info.Name;
		}
	}
	return ret;
}

bool ReplaySaveSlot(
	size_t slot, const std::array<char, (REPLAY_NAME_LENGTH + 1)>& name
)
{
	if((slot >= REPLAY_SLOT_COUNT) || !RecordReady) {
		return false;
	}

	Recorded.Name = name;
	Recorded.Name[REPLAY_NAME_LENGTH] = '\0';
	Recorded.TimestampUTC = static_cast<uint64_t>(
		std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()
		).count()
	);

	const auto metadata = SerializeMetadata(Recorded);
	std::vector<BYTE_BUFFER_GROWABLE> input_files;
	input_files.reserve(Recorded.Stages.size());
	for(const auto& stage : Recorded.Stages) {
		auto& bytes = input_files.emplace_back();
		bytes.reserve(stage.Input.size() * sizeof(uint16_t));
		for(const auto frame : stage.Input) {
			PutLE(bytes, frame);
		}
	}

	PACKFILE_WRITE out;
	out.files.reserve(input_files.size() + 1);
	out.files.emplace_back(std::span(metadata));
	for(const auto& bytes : input_files) {
		out.files.emplace_back(std::span(bytes));
	}

	SDL_CreateDirectory(u8"replay");
	const auto temporary = ReplayFilename(slot, true);
	const auto destination = ReplayFilename(slot);
	if(!out.Write(temporary.c_str())) {
		std::error_code ignored;
		std::filesystem::remove(temporary, ignored);
		SetError("Could not write the replay file.");
		return false;
	}

	if(!SDL_RenamePath(
		reinterpret_cast<const char *>(temporary.c_str()),
		reinterpret_cast<const char *>(destination.c_str())
	)) {
		std::error_code ignored;
		std::filesystem::remove(temporary, ignored);
		SetError("Could not replace the replay slot.");
		return false;
	}
	RecordReady = false;
	return true;
}

REPLAY_SLOT_INFO ReplayScanSlot(size_t slot)
{
	if(slot >= REPLAY_SLOT_COUNT) {
		return { .State = REPLAY_SLOT_STATE::CORRUPT };
	}
	const auto filename = ReplayFilename(slot);
	std::error_code exists_error;
	if(!std::filesystem::exists(filename, exists_error)) {
		if(exists_error) {
			return { .State = REPLAY_SLOT_STATE::CORRUPT };
		}
		return {};
	}

	REPLAY_RUN run;
	switch(LoadReplayFile(filename, run, false)) {
		case LOAD_RESULT::OK:
			return SlotInfo(run);
		case LOAD_RESULT::INCOMPATIBLE:
			return { .State = REPLAY_SLOT_STATE::INCOMPATIBLE };
		default:
			return { .State = REPLAY_SLOT_STATE::CORRUPT };
	}
}

std::array<REPLAY_SLOT_INFO, REPLAY_SLOT_COUNT> ReplayScanSlots(void)
{
	std::array<REPLAY_SLOT_INFO, REPLAY_SLOT_COUNT> ret;
	for(size_t slot = 0; slot < ret.size(); slot++) {
		ret[slot] = ReplayScanSlot(slot);
	}
	return ret;
}

bool ReplayPlaybackBegin(size_t slot, uint8_t stage)
{
	if(slot >= REPLAY_SLOT_COUNT) {
		return false;
	}

	REPLAY_RUN run;
	if(LoadReplayFile(ReplayFilename(slot), run, false) != LOAD_RESULT::OK) {
		SetError("Replay slot is corrupt or incompatible.");
		return false;
	}
	const auto found = std::ranges::find(
		run.Stages, stage, &REPLAY_STAGE::Stage
	);
	if(found == run.Stages.end()) {
		SetError("The selected stage is not in this replay.");
		return false;
	}
	const auto stage_index = static_cast<size_t>(
		std::distance(run.Stages.begin(), found)
	);

	RestorePlaybackConfig();
	PlaybackConfig = {
		.Difficulty = ConfigDat.GameLevel.v,
		.PlayerStock = ConfigDat.PlayerStock.v,
		.BombStock = ConfigDat.BombStock.v,
		.InputFlags = ConfigDat.InputFlags.v,
		.GraphicsFlags = ConfigDat.GraphFlags.v,
		.Valid = true,
	};
	Playing = std::move(run);
	PlaybackStageIndex = stage_index;
	PlaybackFrame = 0;
	Playback = true;
	ApplySnapshot(Playing.Stages[PlaybackStageIndex].Snapshot);
	LastError.clear();
	return true;
}

void ReplayPlaybackApplyStart(void)
{
	if(Playback) {
		ApplySnapshot(Playing.Stages[PlaybackStageIndex].Snapshot);
	}
}

INPUT_BITS ReplayPlaybackInput(void)
{
	if(!Playback) {
		return 0;
	}
	auto& stage = Playing.Stages[PlaybackStageIndex];
	if(PlaybackFrame >= stage.Input.size()) {
		SetError("Replay input ended before the stage finished.");
		Playback = false;
		RestorePlaybackConfig();
		return KEY_ESC;
	}
	return stage.Input[PlaybackFrame++];
}

bool ReplayPlaybackInputExhausted(void)
{
	if(!Playback) {
		return true;
	}
	return (
		PlaybackFrame >= Playing.Stages[PlaybackStageIndex].Input.size()
	);
}

bool ReplayPlaybackStageBegin(uint8_t stage)
{
	if(!Playback) {
		return false;
	}
	const auto& previous = Playing.Stages[PlaybackStageIndex];
	if(CurrentStateHash() != previous.EndHash) {
		SetError("Replay desynchronized at the end of the stage.");
		Playback = false;
		RestorePlaybackConfig();
		return false;
	}
	if((PlaybackStageIndex + 1) >= Playing.Stages.size()) {
		SetError("Replay has no later stage.");
		Playback = false;
		RestorePlaybackConfig();
		return false;
	}

	PlaybackStageIndex++;
	PlaybackFrame = 0;
	const auto& next = Playing.Stages[PlaybackStageIndex];
	if(
		(next.Stage != stage) ||
		(CurrentStateHash() != next.StartHash)
	) {
		SetError("Replay desynchronized at the next stage.");
		Playback = false;
		RestorePlaybackConfig();
		return false;
	}
	return true;
}

bool ReplayPlaybackFinish(void)
{
	if(!Playback) {
		return false;
	}
	const bool synchronized = (
		CurrentStateHash() == Playing.Stages[PlaybackStageIndex].EndHash
	);
	if(!synchronized) {
		SetError("Replay desynchronized at the end of playback.");
	}
	Playback = false;
	RestorePlaybackConfig();
	return synchronized;
}

void ReplayPlaybackAbort(void)
{
	Playback = false;
	RestorePlaybackConfig();
}

bool ReplayPlaybackActive(void)
{
	return Playback;
}

bool ReplayPlaybackHasStage(uint8_t stage)
{
	if(!Playback) {
		return false;
	}
	return std::ranges::any_of(
		Playing.Stages, [stage](const auto& item) {
			return (item.Stage == stage);
		}
	);
}

uint8_t ReplayPlaybackFirstStage(void)
{
	return Playing.Stages.empty() ? 0 : Playing.Stages.front().Stage;
}

uint8_t ReplayPlaybackLastStage(void)
{
	return Playing.Stages.empty() ? 0 : Playing.Stages.back().Stage;
}

const char* ReplayLastError(void)
{
	return LastError.c_str();
}
