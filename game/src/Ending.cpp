#include "Ending.hpp"
#include <stdlib.h>

namespace th08
{

i32 Ending::ReadEndFileParameter()
{
    i32 parameter;

    parameter = atol(this->endFileCursor);

    while (*this->endFileCursor)
    {
        this->endFileCursor++;
    }

    while (!*this->endFileCursor)
    {
        this->endFileCursor++;
    }

    return parameter;
}

void Ending::FadingEffect()
{
}

ZunResult Ending::ParseEndFile()
{
    return ZUN_ERROR;
}

ZunResult Ending::LoadEnding(const char *path)
{
    return ZUN_ERROR;
}

ZunResult Ending::RegisterChain()
{
    return ZUN_ERROR;
}

ChainCallbackResult Ending::OnUpdate(Ending *ending)
{
    i32 i;
    i32 j;

    i = 0;

    while (true)
    {
        if (ending->ParseEndFile())
        {
            return CHAIN_CALLBACK_RESULT_CONTINUE_AND_REMOVE_JOB;
        }

        for (j = 0; j < 15; j++)
        {
            g_AnmManager->ExecuteScript((AnmVm *)((u8 *)ending + j * sizeof(AnmVm) + 0x14));
        }

        if (*(i32 *)((u8 *)ending + 0x2a58) == 0)
        {
            break;
        }

        if ((g_CurFrameInput & TH_BUTTON_SKIP) == 0)
        {
            break;
        }

        if (i >= 8)
        {
            break;
        }

        i++;
    }

    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult __fastcall Ending::OnDraw(Ending *ending)
{
    i32 i;

    g_AnmManager->FUN_00466d70(0, 0, 0, (i32)ending->x, (i32)ending->y, 640, 480);

    for (i = 0; i < 15; i++)
    {
        g_AnmManager->Draw2D((AnmVm *)((u8 *)ending + i * sizeof(AnmVm) + 0x14));
    }

    ending->FadingEffect();
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ZunResult Ending::AddedCallback(Ending *ending)
{
    return ZUN_ERROR;
}

ZunResult Ending::DeletedCallback(Ending *ending)
{
    g_AnmManager->ReleaseAnm(24);
    g_Supervisor.unk174 = 6;
    g_AnmManager->ReleaseSurface(0);
    g_ZunMemory.Free(*(void **)((u8 *)ending + 0x2a54));
    g_Chain.Cut(*(ChainElem **)((u8 *)ending + 4));
    *(ChainElem **)((u8 *)ending + 4) = NULL;
    g_ZunMemory.RemoveFromRegistry(ending);
    delete ending;

    return ZUN_SUCCESS;
}

} /* namespace th08 */
