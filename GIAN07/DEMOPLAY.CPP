/*                                                                           */
/*   DemoPlay.cpp   デモプレイ処理                                           */
/*                                                                           */
/*                                                                           */

#include <SDL3/SDL_iostream.h>

#include "platform/file.h"
#include "CONFIG.H"
#include "GIAN.H"
#include "DEMOPLAY.H"
#include "game/input.h"
#include "game/ut_math.h"


bool	DemoplaySaveEnable = false;	// デモプレイのセーブが動作しているか
bool	DemoplayLoadEnable = false;	// デモプレイのロードが動作しているか
DEMOPLAY_INFO	DemoInfo;						// デモプレイ情報
INPUT_BITS	DemoBuffer[DEMOBUF_MAX];	// デモプレイ用バッファ
static uint32_t DemoFrameCur;
struct {
	uint8_t PlayerStock;
	uint8_t BombStock;
	uint8_t InputFlags;
} ConfigTemp; // コンフィグのデータ一時保存用

//DWORD RndBuf[RNDBUF_MAX];


void DemoplayInit(void)
{
	// 乱数の準備 //
	DemoInfo.RndSeed = (
		(Cast::up<uint32_t>(rnd()) + 1u) * (Cast::up<uint32_t>(rnd()) + 1u)
	);
	rnd_seed_set(DemoInfo.RndSeed);

	DemoInfo.Exp    = Viv.exp;
	DemoInfo.Weapon = Viv.weapon;
	DemoInfo.CfgDat.GameLevel = GameLevel;
	DemoInfo.CfgDat.PlayerStock = Viv.left;
	DemoInfo.CfgDat.BombStock = ConfigDat.BombStock.v;
	DemoInfo.CfgDat.InputFlags = ConfigDat.InputFlags.v;

	DemoFrameCur = 0;
	DemoplaySaveEnable = true;
}


bool DemoplayLoadSetup()
{
	DemoFrameCur = 0;
	DemoplayLoadEnable = true;

	// コンフィグの初期化 //
	// 現在のコンフィグを保持する //
	ConfigTemp.PlayerStock = ConfigDat.PlayerStock.v;
	ConfigTemp.BombStock   = ConfigDat.BombStock.v;
	ConfigTemp.InputFlags  = ConfigDat.InputFlags.v;

	// そのときのコンフィグを転送 //
	ConfigDat.BombStock.v   = DemoInfo.CfgDat.BombStock;
	ConfigDat.PlayerStock.v = DemoInfo.CfgDat.PlayerStock;
	ConfigDat.InputFlags.v  = DemoInfo.CfgDat.InputFlags;
	GameLevel = DemoInfo.CfgDat.GameLevel;

	// 本体の性能記述 //
	Viv.exp    = DemoInfo.Exp;
	Viv.weapon = DemoInfo.Weapon;
	Viv.left   = ConfigDat.PlayerStock.v;
	Viv.bomb   = ConfigDat.BombStock.v;

	// 乱数の初期化 //
	// 最後に乱数もそろえる //
	rnd_seed_set(DemoInfo.RndSeed);

	return true;
}


bool DemoplayRecord(INPUT_BITS key)
{
	if(!DemoplaySaveEnable) {
		return false;
	}

	DemoBuffer[DemoFrameCur++] = key;

	// バッファが最後に来たか、ＥＳＣが押された場合 //
	if((DemoFrameCur == DEMOBUF_MAX) || (key & KEY_ESC)) {
		DemoFrameCur--;
		return true;
	}
	return false;
}


void DemoplaySaveDemo(void)
{
	if(!DemoplaySaveEnable) return;

	DemoBuffer[DemoFrameCur] = KEY_ESC;
	DemoInfo.FrameCount = (DemoFrameCur + 1);

	char8_t fn[] = u8"STG_Demo.DAT";
	fn[3] = ('0' + GameStage);

	auto *f = SDL_IOFromFile(fn, "wb");
	if(f) {
		SDL_WriteIO(f, &DemoInfo, sizeof(DemoInfo));
		SDL_WriteIO(
			f, DemoBuffer, (sizeof(DemoBuffer[0]) * DemoInfo.FrameCount)
		);
		SDL_CloseIO(f);
	}

	DemoplaySaveEnable = false;
}


bool DemoplayLoadDemo(int stage)
{
	// 展開 //
	const auto temp = LoadDemo(stage);
	auto temp_cursor = temp.cursor();
	{
		const auto maybe_info = temp_cursor.next<DEMOPLAY_INFO>();
		if(!maybe_info) {
			return false;
		}
		DemoInfo = maybe_info.value()[0];
	}
	{
		const auto maybe_inputs = temp_cursor.next<uint16_t>(DemoInfo.FrameCount);
		if(!maybe_inputs) {
			return false;
		}
		const auto inputs = maybe_inputs.value();
		memcpy(DemoBuffer, inputs.data(), inputs.size_bytes());
	}
	return DemoplayLoadSetup();
}


INPUT_BITS DemoplayMove(void)
{
	if(!DemoplayLoadEnable) return KEY_ESC;

	const auto ptr = DemoFrameCur;
	if(ptr>=DemoInfo.FrameCount){
		DemoplayLoadEnable = false;
	}
	else DemoFrameCur++;

	return DemoBuffer[ptr];
}


void DemoplayCleanup(void)
{
	ConfigDat.PlayerStock.v = ConfigTemp.PlayerStock;
	ConfigDat.BombStock.v   = ConfigTemp.BombStock;
	ConfigDat.InputFlags.v  = ConfigTemp.InputFlags;

	DemoplayLoadEnable = false;
}
