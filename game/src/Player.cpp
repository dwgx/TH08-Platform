#include "Player.hpp"
#include "AnmManager.hpp"
#include "AsciiManager.hpp"

namespace th08
{

DIFFABLE_STATIC(Player, g_Player);
DIFFABLE_STATIC(ChainElem *, g_PlayerCalcChain);
DIFFABLE_STATIC(ChainElem *, g_PlayerDrawChainHighPrio);
DIFFABLE_STATIC(ChainElem *, g_PlayerDrawChainLowPrio);

ZunResult Player::RegisterChain(u32 param)
{
    return ZUN_SUCCESS;
}

ChainCallbackResult Player::OnUpdate(Player *player)
{
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult Player::OnDrawHighPrio(Player *player)
{
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ZunResult Player::AddedCallback(Player *player)
{
    return ZUN_SUCCESS;
}

ZunResult __fastcall Player::DeletedCallback(Player *player)
{
    if (player->FUN_004338c0())
    {
        g_AnmManager->ReleaseAnm(5);
        g_AsciiManager.SetGaugeInterrupt(99);
        g_AsciiManager.FUN_00422bb0(0, 99);
        g_AsciiManager.FUN_00422bb0(1, 99);
        g_AsciiManager.FUN_00422bb0(2, 99);
        if (*(void **)0x18B896C != NULL)
        {
            g_ZunMemory.Free(*(void **)0x18B896C);
            *(void **)0x18B896C = NULL;
        }
        if (*(void **)0x18B8970 != NULL)
        {
            g_ZunMemory.Free(*(void **)0x18B8970);
            *(void **)0x18B8970 = NULL;
        }
    }
    return ZUN_SUCCESS;
}

void Player::CutChain()
{
    g_Chain.Cut(g_PlayerCalcChain);
    g_PlayerCalcChain = NULL;
    g_Chain.Cut(g_PlayerDrawChainHighPrio);
    g_PlayerDrawChainHighPrio = NULL;
    g_Chain.Cut(g_PlayerDrawChainLowPrio);
    g_PlayerDrawChainLowPrio = NULL;
}

u32 Player::CalcItemBoxCollision(D3DXVECTOR3 *size, D3DXVECTOR3 *center)
{
    D3DXVECTOR3 min;
    D3DXVECTOR3 max;
    f32 *player;

    if (*(i8 *)this != 0 && *(i8 *)this != 3 && *(i8 *)this != 4)
    {
        return FALSE;
    }

    min = *center - *size / 2.0f;
    max = *center + *size / 2.0f;
    player = (f32 *)this;

    return player[239] <= max.x && player[242] >= min.x && player[240] <= max.y && player[243] >= min.y;
}

BOOL Player::FUN_004338c0()
{
    return FALSE;
}

ZunResult Player::LoadShtFile(PlayerRawShtFile **header, const char *path)
{
    return ZUN_SUCCESS;
}

} /* namespace th08 */
