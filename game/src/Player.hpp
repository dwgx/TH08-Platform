#pragma once

#include "Global.hpp"

namespace th08
{

struct PlayerRawShtFile
{
};

enum PlayerState
{
    PLAYER_STATE_ALIVE,
    PLAYER_STATE_SPAWNING,
    PLAYER_STATE_DEAD,
};

struct Player
{
    i8 playerState;

    static ZunResult RegisterChain(u32 param);
    static ChainCallbackResult OnUpdate(Player *player);
    static ChainCallbackResult OnDrawHighPrio(Player *player);
    static ZunResult AddedCallback(Player *player);
    static ZunResult __fastcall DeletedCallback(Player *player);
    static void CutChain();
    u32 CalcItemBoxCollision(D3DXVECTOR3 *size, D3DXVECTOR3 *center);
    BOOL FUN_004338c0();

    static ZunResult LoadShtFile(PlayerRawShtFile **header, const char *path);
};

DIFFABLE_EXTERN(Player, g_Player);

} /* namespace th08 */
