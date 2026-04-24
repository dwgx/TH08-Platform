#include "Background.hpp"
#include "AnmManager.hpp"

namespace th08
{
DIFFABLE_STATIC(Background, g_Background);
DIFFABLE_STATIC(ChainElem, g_BackgroundDrawChainHighPrio);
DIFFABLE_STATIC(ChainElem, g_BackgroundDrawChainLowPrio);
DIFFABLE_STATIC(ChainElem, g_BackgroundCalcChain);

ZunBool IsDisableResourceReload()
{
    return FALSE;
}

Background::Background()
{
}

ChainCallbackResult Background::OnUpdate(Background *background)
{
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult Background::OnDrawHighPrio(Background *background)
{
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult Background::OnDrawLowPrio(Background *background)
{
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ZunResult Background::AddedCallback(Background *background)
{
    return ZUN_ERROR;
}

ZunResult Background::RegisterChain()
{
    return ZUN_ERROR;
}

ZunResult __fastcall Background::DeletedCallback(Background *background)
{
    if (!IsDisableResourceReload())
    {
        g_AnmManager->ReleaseAnm(4);
    }

    if (background->field_0 != NULL)
    {
        g_ZunMemory.Free(background->field_0);
        background->field_0 = NULL;
    }

    if (!IsDisableResourceReload() && background->field_7f4 != NULL)
    {
        g_ZunMemory.Free(background->field_7f4);
        background->field_7f4 = NULL;
    }

    return ZUN_SUCCESS;
}

void Background::CutChain()
{
    g_Chain.Cut(&g_BackgroundCalcChain);
    g_Chain.Cut(&g_BackgroundDrawChainHighPrio);
    g_Chain.Cut(&g_BackgroundDrawChainLowPrio);
}

ZunResult Background::LoadStageData()
{
    return ZUN_ERROR;
}

}; // Namespace th08
