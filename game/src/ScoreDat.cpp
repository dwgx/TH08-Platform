#include "ScoreDat.hpp"

namespace th08
{

i32 __fastcall ScoreDat::LinkScore(ScoreListNode *prevNode, Hscr *newScore)
{
    return 0;
}

void __fastcall ScoreDat::FreeAllScores(ScoreListNode *score)
{
    ScoreListNode *block;
    ScoreListNode *next;

    block = *(ScoreListNode **)((u8 *)score + 4);
    while (block != NULL)
    {
        next = *(ScoreListNode **)((u8 *)block + 4);
        g_ZunMemory.Free(block);
        block = next;
    }
}

ScoreDat *ScoreDat::OpenScore(const char *filename)
{
    return NULL;
}

u32 ScoreDat::GetHighScore(ScoreDat *score, ScoreListNode *node, u32 character, u32 difficulty, u8 *continuesUsed)
{
    return 0;
}

i32 ScoreDat::ParseCATK(ScoreDat *score, Catk *outCatk)
{
    return 0;
}

i32 ScoreDat::ParseLSNM(ScoreDat *score, Lsnm *outLsnm)
{
    return 0;
}

i32 ScoreDat::ParseFLSP(ScoreDat *score, Flsp *outFlsp)
{
    return 0;
}

i32 ScoreDat::ParseCLRD(ScoreDat *score, Clrd *outClrd)
{
    return 0;
}

i32 ScoreDat::ParsePSCR(ScoreDat *score, Pscr *outPscr)
{
    return 0;
}

i32 ScoreDat::ParsePLST(ScoreDat *score, Plst *outPlst)
{
    return 0;
}

void ScoreDat::ReleaseScore(ScoreDat *score)
{
    ScoreDat::FreeAllScores(*(ScoreListNode **)((u8 *)score + 0xc));
    g_ZunMemory.Free(*(void **)((u8 *)score + 0xc));
    g_ZunMemory.Free(score);
}

} /* namespace th08 */
