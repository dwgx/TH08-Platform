#include "ResultScreen.hpp"
#include "ScoreDat.hpp"
#include <time.h>

namespace th08
{

DIFFABLE_STATIC_ARRAY_ASSIGN(const char *, 12, g_CharacterList) = {"Yuyuko ", "Youmu  ", "Remilia", "Sakuya ",
                                                                    "Alice  ", "Marisa ", "Yukari ", "Reimu  ",
                                                                    "Ym & Yy", "Sk & Rr", "Ms & Al", "Rm & Yk"};
DIFFABLE_STATIC_ARRAY_ASSIGN(const char *, 9, g_ResultsStageList) = {"Extra Stage",      "Stage 6-Kaguya",
                                                                      "Stage 6-Eirin",    "Stage 5",
                                                                      "Stage 4-powerful", "Stage 4-uncanny",
                                                                      "Stage 3",          "Stage 2",
                                                                      "Stage 1"};

const char *__fastcall ResultScreen::GetStageName(i32 stage)
{
    i32 stageLocal = stage;
    const char *stageName;

    if (stageLocal >= 9)
    {
        stageName = "Clear";
    }
    else
    {
        stageName = g_ResultsStageList[stageLocal];
    }

    return stageName;
}

const char *__fastcall ResultScreen::GetCharacterName(i32 character)
{
    return g_CharacterList[character];
}

void ResultScreen::WriteScore(ResultScreen *resultScreen)
{
}

void ResultScreen::LogScoreDataToFile(ResultScreen *resultScreen)
{
}

void ResultScreen::LinkScoreEx(void *out, i32 difficulty, i32 character)
{
    ScoreListNode *score = (ScoreListNode *)((u8 *)this + difficulty * 0x90);

    ScoreDat::LinkScore((ScoreListNode *)((u8 *)score + character * 0xc + 0x1144c), (Hscr *)out);
}

void ResultScreen::FreeScore(i32 difficulty, i32 character)
{
    ScoreListNode *score = (ScoreListNode *)((u8 *)this + difficulty * 0x90);

    ScoreDat::FreeAllScores((ScoreListNode *)((u8 *)score + character * 0xc + 0x1144c));
}

void ResultScreen::HandleCategorySelectScreen()
{
}

void ResultScreen::SetState(ResultScreenState state)
{
    this->previousState = this->currentState;
    this->currentState = state;
    this->stateCopy = state;
    this->field_10 = 0;
    this->field_18 = 0;
    this->field_4 = 0;
    this->field_54 = 0;
}

i32 ResultScreen::HandleHighScoreDifficultySelect()
{
    return 0;
}

i32 ResultScreen::HandleHighScoreCharacterSelect()
{
    return 0;
}

i32 ResultScreen::HandleHighScoreScreen()
{
    return 0;
}

i32 ResultScreen::HandleSpellCardDifficultySelect()
{
    return 0;
}

i32 ResultScreen::HandleSpellCardCharacterSelect()
{
    return 0;
}

i32 ResultScreen::HandleSpellCardScreen()
{
    return 0;
}

i32 ResultScreen::HandleResultKeyboard()
{
    return 0;
}

void __fastcall ResultScreen::FormatDate(char *buffer)
{
    struct tm *currentLocalTime;
    time_t currentTime;

    time(&currentTime);
    currentLocalTime = localtime(&currentTime);
    strftime(buffer, 6, "%m/%d", currentLocalTime);
}

i32 ResultScreen::HandleReplaySaveKeyboard()
{
    return 0;
}

ZunResult ResultScreen::CheckConfirmButton()
{
    return ZUN_SUCCESS;
}

i32 ResultScreen::HandleOtherStatsScreen()
{
    return 0;
}

i32 ResultScreen::DrawFinalStats()
{
    return 0;
}

ZunResult ResultScreen::RegisterChain(u32 unk)
{
    return ZUN_SUCCESS;
}

ChainCallbackResult ResultScreen::OnUpdate(ResultScreen *resultScreen)
{
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult ResultScreen::OnDraw(ResultScreen *resultScreen)
{
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ZunResult ResultScreen::AddedCallback(ResultScreen *resultScreen)
{
    return ZUN_SUCCESS;
}

ZunResult ResultScreen::DeletedCallback(ResultScreen *resultScreen)
{
    return ZUN_SUCCESS;
}

i32 ResultScreen::MoveCursor(ResultScreen *resultScreen, i32 length)
{
    return 0;
}
i32 ResultScreen::MoveShotTypeCursor(ResultScreen *resultScreen, i32 length)
{
    return 0;
}

i32 ResultScreen::MoveCursorHorizontally(ResultScreen *resultScreen, int length)
{
    return 0;
}

ResultScreen::~ResultScreen()
{
    g_ZunMemory.Free(this->heapData);
}

} /* namespace th08 */
