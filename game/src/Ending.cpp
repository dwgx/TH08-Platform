#include "Ending.hpp"
#include <stdlib.h>

namespace th08
{

i32 Ending::ReadEndFileParameter()
{
    i32 parameter = atol(this->endFileCursor);

    while (*this->endFileCursor != '\0')
    {
        this->endFileCursor++;
    }

    while (*this->endFileCursor == '\0')
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
    return ZUN_ERROR;
}

} /* namespace th08 */
