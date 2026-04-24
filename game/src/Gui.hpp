#pragma once

#include "Global.hpp"

namespace th08
{

struct GuiImpl
{
    unknown_fields(0x0, 0x21814);
    void *msgFileData;
    unknown_fields(0x21818, 0x4);
    i32 msgState;
    unknown_fields(0x21820, 0x1558);
    u32 waitCounter;
};

struct Gui
{
    static ChainCallbackResult OnUpdate(Gui *gui);
    static ChainCallbackResult OnDraw(Gui *gui);

    static ZunResult __fastcall AddedCallback(Gui *gui);
    static ZunResult DeletedCallback(Gui *gui);

    static ZunResult RegisterChain();
    static void CutChain();

    ZunResult ActualAddedCallback();
    ZunResult LoadMsg(const char *path);
    BOOL MsgWait();
    void FreeMsgFile();

    unknown_fields(0x0, 0x8);
    GuiImpl *impl;
};

DIFFABLE_EXTERN(Gui, g_Gui);

} /* namespace th08 */
