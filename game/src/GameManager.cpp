#include "GameManager.hpp"
#include "Global.hpp"
#include "SoundPlayer.hpp"

namespace th08
{

DIFFABLE_STATIC(GameManager, g_GameManager);
DIFFABLE_STATIC(ChainElem, g_GameManagerCalcChain);
DIFFABLE_STATIC(ChainElem, g_GameManagerDrawChain);
DIFFABLE_STATIC_ARRAY_ASSIGN(i32, 18, g_RankParams) = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

ZunBool GameManager::IsWithinPlayfield()
{
    return FALSE;
}

i32 GameManager::CalcAntiTamperChecksum()
{
    return 0;
}

i32 GameManager::CalcChecksum(u8 *param_1, i32 param_2)
{
    return 0;
}

void GameManager::CollectExtend()
{
    if ((int)(unsigned __int64)this->globals->livesRemaining >= 8)
    {
        if ((int)(unsigned __int64)this->globals->bombsRemaining < 8)
        {
            this->AddToBombCount(1);
            g_SoundPlayer.PlaySoundByIdx((SoundIdx)28, 0);
            this->IncreaseSubrank(200);
            *(u32 *)0x160F42C = *(u32 *)0x160F42C & 0xFFFFFFF3 | 8;
        }
    }
    else
    {
        this->AddLives(1);
        g_SoundPlayer.PlaySoundByIdx((SoundIdx)28, 0);
        this->IncreaseSubrank(200);
        *(u32 *)0x160F42C = *(u32 *)0x160F42C & 0xFFFFFFFC | 2;
    }
}

ChainCallbackResult GameManager::OnUpdate(GameManager *gameManager)
{
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult GameManager::OnDraw(GameManager *gameManager)
{
    if (gameManager->isInGameMenu)
    {
        gameManager->isInGameMenu = 2;
    }
    if (*(i32 *)0x17CE8B4 != 2)
    {
        return CHAIN_CALLBACK_RESULT_BREAK;
    }
    if (((*(u32 *)&gameManager->flags >> 5) & 3) == 1)
    {
        return CHAIN_CALLBACK_RESULT_BREAK;
    }
    if (gameManager->unk38 != 0)
    {
        return CHAIN_CALLBACK_RESULT_BREAK;
    }
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ZunResult GameManager::RegisterChain()
{
    return ZUN_SUCCESS;
}

ZunResult GameManager::AddedCallback(GameManager *gameManager)
{
    return ZUN_SUCCESS;
}

void GameManager::GameplaySetupThread()
{
}
ZunResult GameManager::DeletedCallback(GameManager *gameManager)
{
    return ZUN_SUCCESS;
}

void GameManager::IncreaseSubrank(int amount)
{
}

void GameManager::DecreaseSubrank(int amount)
{
}

void GameManager::AddToYoukaiGauge(i16 amount, i32 forceUpdate)
{
    if (*(i32 *)0x17D6ED4 == 0 || forceUpdate != 0)
    {
        this->globals->youkaiGauge += amount;
        if (this->globals->youkaiGauge >= this->youkaiGaugeHumanLimit)
        {
            if (this->globals->youkaiGauge > this->youkaiGaugeYoukaiLimit)
            {
                this->globals->youkaiGauge = this->youkaiGaugeYoukaiLimit;
            }
        }
        else
        {
            this->globals->youkaiGauge = this->youkaiGaugeHumanLimit;
        }
        this->globals->youkaiGaugeCopy = this->globals->youkaiGauge;
    }
}

ZunBool GameManager::FinalBCleared(i32 team)
{
    return FALSE;
}

ZunBool GameManager::FinalBClearedWithAnyTeam()
{
    GameManager *gameManager = this;
    ZunBool result;

    if (!gameManager->FinalBCleared(0) && !gameManager->FinalBCleared(1) && !gameManager->FinalBCleared(2) &&
        !gameManager->FinalBCleared(3))
    {
        result = FALSE;
    }
    else
    {
        result = TRUE;
    }

    return result;
}

ZunBool GameManager::FinalACleared(i32 team)
{
    return FALSE;
}

ZunBool GameManager::FinalAClearedWithAnyTeam()
{
    GameManager *gameManager = this;
    ZunBool result;

    if (!gameManager->FinalACleared(0) && !gameManager->FinalACleared(1) && !gameManager->FinalACleared(2) &&
        !gameManager->FinalACleared(3))
    {
        result = FALSE;
    }
    else
    {
        result = TRUE;
    }

    return result;
}

ZunBool GameManager::FinalBClearedWithAllTeams()
{
    GameManager *gameManager = this;
    ZunBool result;

    if (gameManager->FinalBCleared(0) && gameManager->FinalBCleared(1) && gameManager->FinalBCleared(2) &&
        gameManager->FinalBCleared(3))
    {
        result = TRUE;
    }
    else
    {
        result = FALSE;
    }

    return result;
}

void GameManager::CutChain()
{
    g_Chain.Cut(&g_GameManagerCalcChain);
    g_Chain.Cut(&g_GameManagerDrawChain);
    if (g_GameManager.globals->score >= 1000000000)
    {
        g_GameManager.globals->score = 999999999;
    }
    g_GameManager.globals->displayScore = g_GameManager.globals->score;
    g_Supervisor.framerateMultiplier = 1.0f;
}

i32 GameManager::GetClockIncrement()
{
    return 0;
}

void GameManager::AdvanceToNextStage()
{
}

void GameManager::SetLives(i32 lives)
{
    this->globals->livesRemaining = lives;
    this->UpdateAntiTamper();
}

u32 GameManager::GetFlag14()
{
    return (*(u32 *)&this->flags >> 14) & 1;
}

u32 GameManager::GetFlag3()
{
    return (*(u32 *)&this->flags >> 3) & 1;
}

u32 GameManager::GetFlag1()
{
    return (*(u32 *)&this->flags >> 1) & 1;
}

u32 GameManager::GetFlag0()
{
    return *(u32 *)&this->flags & 1;
}

i32 GameManager::GetYoukaiGauge()
{
    return this->globals->youkaiGauge;
}

bool GameManager::GaugeIsExtremelyHuman()
{
    return this->globals->youkaiGauge <= this->youkaiGaugeHumanEffectsThreshold;
}

bool GameManager::GaugeIsModeratelyHuman()
{
    return this->globals->youkaiGauge <= this->youkaiGaugeHumanTintThreshold;
}

bool GameManager::GaugeIsExtremelyYoukai()
{
    return this->globals->youkaiGauge >= this->youkaiGaugeYoukaiEffectsThreshold;
}

bool GameManager::GaugeIsModeratelyYoukai()
{
    return this->globals->youkaiGauge >= this->youkaiGaugeYoukaiTintThreshold;
}

u8 GameManager::GetClockTime()
{
    return this->globals->clockTime;
}

void GameManager::SetClockTime(u8 clockTime)
{
    this->globals->clockTime = clockTime;
}

void GameManager::AddToClockTime(char clockTime)
{
    i32 result;

    result = clockTime + (char)this->globals->clockTime;
    this->globals->clockTime = result;
}

unsigned __int64 GameManager::GetDeaths()
{
    return (unsigned __int64)this->globals->deaths;
}

u8 GameManager::SetPower(i32 power)
{
    this->globals->playerPower = (f32)power;
    return this->UpdateAntiTamper();
}

void GameManager::AddScore(i32 score)
{
    this->globals->score += score / 10;
}

u32 GameManager::GetTimeOrbs()
{
    return this->globals->currentTimeOrbs;
}

u32 GameManager::GetLastSpellTimeOrbThreshold()
{
    return this->globals->lastSpellTimeOrbThreshold;
}

i32 GameManager::ScaleIntBasedOnRank(i32 min, i32 max)
{
    return this->rank * (max - min) / 32 + min;
}

unsigned __int64 GameManager::GetPower()
{
    return (unsigned __int64)this->globals->playerPower;
}

f64 GameManager::ScaleFloatBasedOnRank(f32 min, f32 max)
{
    return this->rank * (max - min) / 32.0f + min;
}

i32 GameManager::IsSoloHuman()
{
    return this->shotType >= 4 && (this->shotType & 1) == 0;
}

i32 GameManager::IsSoloYoukai()
{
    return this->shotType >= 4 && (this->shotType & 1) != 0;
}

unsigned __int64 GameManager::GetLives()
{
    return (unsigned __int64)this->globals->livesRemaining;
}

void GameManager::SetYoukaiGauge(u16 youkaiGauge)
{
    this->globals->youkaiGauge = youkaiGauge;
}

void GameManager::AddToBombCount(i32 bombs)
{
    if (this->IsTampered())
    {
        CRASH_GAME();
    }
    this->globals->bombsRemaining = bombs + this->globals->bombsRemaining;
    this->UpdateAntiTamper();
}

unsigned __int64 GameManager::GetBombsRemaining()
{
    ZunGlobals *globals;

    globals = this->globals;
    return (unsigned __int64)globals->bombsRemaining;
}

unsigned __int64 GameManager::GetBombsUsed()
{
    return (unsigned __int64)this->globals->bombsUsed;
}

void GameManager::InitRankParams()
{
    this->rank = g_RankParams[3 * g_Supervisor.cfg.defaultDifficulty];
    this->minRank = g_RankParams[3 * g_Supervisor.cfg.defaultDifficulty + 1];
    this->maxRank = g_RankParams[3 * g_Supervisor.cfg.defaultDifficulty + 2];
}

u8 GameManager::AddLives(i32 lives)
{
    if (this->IsTampered())
    {
        CRASH_GAME();
    }
    this->globals->livesRemaining = lives + this->globals->livesRemaining;
    return this->UpdateAntiTamper();
}

void GameManager::AddPower(int power)
{
    if (this->IsTampered())
    {
        CRASH_GAME();
    }
    this->globals->playerPower = power + this->globals->playerPower;
    this->UpdateAntiTamper();
}

u8 GameManager::UpdateAntiTamper()
{
    return 0;
}

ZunBool GameManager::IsTampered()
{
    return FALSE;
}

GameManager::GameManager()
{
    memset(this, 0, sizeof(GameManager));
}

void GameManager::InitArcadeRegionParams()
{
}

}; // Namespace th08
