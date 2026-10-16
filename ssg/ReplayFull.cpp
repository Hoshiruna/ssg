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

struct Writer {
	BUFFER_GROWABLE Bytes;

	void Put(uint64_t value, size_t count)
	{
		for(size_t i = 0; i < count; i++) {
			Bytes.push_back(static_cast<uint8_t>(value >> (i * 8)));
		}
	}
};

struct Reader {
	BUFFER_BORROWED Bytes;
	size_t At = 0;
	bool Valid = true;

	uint64_t Get(size_t count)
	{
		if(count > Bytes.size() - At) {
			Valid = false;
			return 0;
		}
		uint64_t value = 0;
		for(size_t i = 0; i < count; i++) {
			value |= uint64_t(Bytes[At++]) << (i * 8);
		}
		return value;
	}
};

const char* Difference(const Checkpoint& expected, const Checkpoint& actual)
{
	if(expected.Steps != actual.Steps) {
		return "checkpoint: Steps differ";
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

bool ValidStart(const FullData& d)
{
	return (
		((d.GameMode == Mode::Main && d.FirstStage == 1) ||
		 (d.GameMode == Mode::Extra && d.FirstStage == STAGE_EXTRA) ||
		 (d.GameMode == Mode::Practice && d.FirstStage >= 1 && d.FirstStage <= STAGE_MAX)) &&
		d.Round.LevelSelected <= GAME_LUNATIC &&
		d.Round.Weapon < 3 &&
		d.Round.MsgEnabled <= 1
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
	Reader r{static_cast<BUFFER_BORROWED>(header)};
	return r.Get(4) == Magic && r.Valid;
}

BUFFER_GROWABLE FullEncode(const FullData& d)
{
	if(
		!ValidStart(d) ||
		d.Records.size() > (256u * 1024u * 1024u) / sizeof(Record)
	) {
		return {};
	}

	Writer w;
	w.Put(Magic, 4);
	w.Put(FormatVersion, 2);
	w.Put(LogicVersion, 2);
	w.Put(std::to_underlying(d.GameMode), 1);
	w.Put(d.FirstStage, 1);
	w.Put(d.Round.Seed, 4);
	for(auto v : {
		d.Round.LevelSelected,
		d.Round.PlayerStock,
		d.Round.BombStock,
		d.Round.InputFlags,
		d.Round.Weapon,
		d.Round.Exp,
		uint8_t(d.Round.MsgEnabled)
	}) {
		w.Put(v, 1);
	}
	w.Put(d.Records.size(), 8);

	for(const auto& r : d.Records) {
		w.Put(std::to_underlying(r.Kind), 1);
		w.Put(r.Value, 2);
		if(r.Kind == Operation::Checkpoint || r.Kind == Operation::End) {
			const auto& s = r.State;
			w.Put(s.Steps, 8);
			w.Put(static_cast<uint64_t>(s.Score), 8);
			w.Put(s.Seed, 4);
			w.Put(static_cast<uint32_t>(s.Rank), 4);
			for(auto v : {s.Stage, s.Lives, s.Bombs, s.Power}) {
				w.Put(v, 1);
			}
		}
	}
	if(w.Bytes.size() > 256u * 1024u * 1024u) {
		return {};
	}
	const BUFFER_BORROWED payload{w.Bytes};
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
	const auto records = dat.MemExpand(1);
	if(!header || !records) {
		return "invalid DAT compressed replay data";
	}
	BUFFER_GROWABLE expanded(header.get(), header.get() + header.size());
	expanded.insert(expanded.end(), records.get(), records.get() + records.size());
	bytes = BUFFER_BORROWED{expanded};
	Reader r{bytes};
	if(r.Get(4) != Magic) {
		return "invalid replay signature";
	}
	if(r.Get(2) != FormatVersion) {
		return "unsupported replay format version";
	}
	if(r.Get(2) != LogicVersion) {
		return "incompatible replay logic version";
	}
	FullData d;
	d.GameMode = static_cast<Mode>(r.Get(1));
	d.FirstStage = static_cast<uint8_t>(r.Get(1));
	d.Round.Seed = static_cast<uint32_t>(r.Get(4));
	for(auto p : {
		&d.Round.LevelSelected,
		&d.Round.PlayerStock,
		&d.Round.BombStock,
		&d.Round.InputFlags,
		&d.Round.Weapon,
		&d.Round.Exp,
		&d.Round.MsgEnabled
	}) {
		*p = static_cast<uint8_t>(r.Get(1));
	}

	const auto count = r.Get(8);
	if(
		!r.Valid || !ValidStart(d) ||
		count > (bytes.size() - r.At) / 3 ||
		count > (256u * 1024u * 1024u) / sizeof(Record)
	) {
		return "invalid replay header or record count";
	}

	for(uint64_t i = 0; i < count; i++) {
		Record v;
		v.Kind = static_cast<Operation>(r.Get(1));
		v.Value = static_cast<uint16_t>(r.Get(2));
		if(v.Kind > Operation::End) {
			return "unknown replay operation";
		}
		if(v.Kind == Operation::Checkpoint || v.Kind == Operation::End) {
			auto& s = v.State;
			s.Steps = r.Get(8);
			s.Score = std::bit_cast<int64_t>(r.Get(8));
			s.Seed = static_cast<uint32_t>(r.Get(4));
			s.Rank = std::bit_cast<int32_t>(static_cast<uint32_t>(r.Get(4)));
			for(auto p : {&s.Stage, &s.Lives, &s.Bombs, &s.Power}) {
				*p = static_cast<uint8_t>(r.Get(1));
			}
		}
		if(!r.Valid) {
			return "truncated replay record";
		}
		if((v.Kind == Operation::End) != (i + 1 == count)) {
			return "replay must end with one End record";
		}
		d.Records.push_back(v);
	}
	if(count == 0 || r.At != bytes.size()) {
		return "missing end or trailing replay data";
	}

	out = std::move(d);
	return nullptr;
}

Checkpoint Full::Capture() const
{
	return {
		Steps,
		Game.Viv.score,
		Game.RNG.seed,
		Game.PlayRank.Rank(),
		Game.Stage.Number(),
		Game.Viv.left,
		Game.Viv.bomb,
		Game.Viv.exp
	};
}

const char* Full::Start(const FullData& data)
{
	Ended = true;
	if(!ValidStart(data)) {
		return "invalid starting parameters";
	}
	GameMode = data.GameMode;
	Steps = 0;
	Boundary = Cleared = GameOver = false;
	Game.RoundInit(data.Round, data.FirstStage);
	if(!Game.StageLoadFromDAT(Resources, data.FirstStage)) {
		return "stage load failed";
	}
	Ended = false;
	return nullptr;
}

const char* Full::Apply(const Record& r)
{
	if(Ended) {
		return "operation after replay end";
	}
	if(Boundary && r.Kind != Operation::Checkpoint) {
		return "missing stage checkpoint";
	}

	switch(r.Kind) {
	case Operation::Frame:
		if(Cleared || GameOver || Game.Viv.GameOverTimer || (r.Value & ~INPUT_BITS(0x2ff))) {
			return "invalid gameplay frame";
		}
		Game.Move(r.Value);
		Steps++;
		GameOver = Game.Viv.GameOverTimer > 0;
		Boundary = Game.Stage.GetEndCommand() != 0;
		return nullptr;

	case Operation::GameOverFrame:
		if(r.Value || Cleared || Game.Viv.GameOverTimer <= 0) {
			return "invalid Game Over frame";
		}
		Game.MoveGameOver();
		Steps++;
		return nullptr;

	case Operation::Continue:
		if(r.Value || Cleared || Game.Viv.GameOverTimer != 0 || !GameOver || !Game.Viv.credit) {
			return "invalid continue";
		}
		Game.RoundContinue();
		GameOver = false;
		return nullptr;

	case Operation::Checkpoint:
		if(!Boundary || r.Value) {
			return "unexpected checkpoint";
		}
		if(const auto error = Difference(r.State, Capture())) {
			return error;
		}
		Boundary = false;
		if(
			GameMode == Mode::Main &&
			Game.Stage.GetEndCommand() == SCL_STAGECLEAR &&
			Game.Stage.Number() < STAGE_MAX
		) {
			if(!Game.StageLoadFromDAT(Resources, Game.Stage.Number() + 1)) {
				Ended = true;
				return "stage load failed";
			}
		} else {
			Cleared = true;
		}
		return nullptr;

	case Operation::End:
		if(r.Value > std::to_underlying(EndReason::Quit)) {
			return "invalid end reason";
		}
		if(r.Value == std::to_underlying(EndReason::Clear) && !Cleared) {
			return "clear before stage completion";
		}
		if(
			r.Value == std::to_underlying(EndReason::GameOver) &&
			(!GameOver || Game.Viv.GameOverTimer != 0)
		) {
			return "premature Game Over end";
		}
		if(const auto error = Difference(r.State, Capture())) {
			return error;
		}
		Ended = true;
		return nullptr;
	}
	return "unknown replay operation";
}

} // namespace Replay
