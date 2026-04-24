#include "Gui.hpp"

namespace th08
{

DIFFABLE_STATIC(Gui, g_Gui);
DIFFABLE_STATIC(ChainElem, g_GuiCalcChain);
DIFFABLE_STATIC(ChainElem, g_GuiDrawChain);

ChainCallbackResult Gui::OnUpdate(Gui *gui)
{
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult Gui::OnDraw(Gui *gui)
{
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ZunResult __fastcall Gui::AddedCallback(Gui *gui)
{
    return gui->ActualAddedCallback();
}

ZunResult Gui::DeletedCallback(Gui *gui)
{
    return ZUN_SUCCESS;
}

ZunResult Gui::RegisterChain()
{
    return ZUN_SUCCESS;
}

void Gui::CutChain()
{
    g_Chain.Cut(&g_GuiCalcChain);
    g_Chain.Cut(&g_GuiDrawChain);
}

ZunResult Gui::ActualAddedCallback()
{
    return ZUN_SUCCESS;
}

ZunResult Gui::LoadMsg(const char *path)
{
    return ZUN_SUCCESS;
}

BOOL Gui::MsgWait()
{
    if (this->impl == NULL)
    {
        return FALSE;
    }

    if (this->impl->waitCounter > 0)
    {
        return FALSE;
    }

    return this->impl->msgState >= 0;
}

void Gui::FreeMsgFile(void)
{
    if (this->impl->msgFileData != NULL)
    {
        g_ZunMemory.Free(this->impl->msgFileData);
        this->impl->msgFileData = NULL;
    }
}

} /* namespace th08 */
