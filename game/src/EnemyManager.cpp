#include "EnemyManager.hpp"
#include "AnmManager.hpp"

namespace th08
{

DIFFABLE_STATIC(EnemyManager, g_EnemyManager);
DIFFABLE_STATIC(ChainElem, g_EnemyManagerCalcChain);
DIFFABLE_STATIC(ChainElem, g_EnemyManagerDrawChainHighPrio);
DIFFABLE_STATIC(ChainElem, g_EnemyManagerDrawChainLowPrio);

void EnemyManager::Initialize()
{
}

ZunResult EnemyManager::RegisterChain()
{
    return ZUN_ERROR;
}

ChainCallbackResult EnemyManager::OnUpdate()
{
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult EnemyManager::OnDrawHighPrio(EnemyManager *enemyManager)
{
    return enemyManager->OnDrawImpl(0, 2);
}

ChainCallbackResult __fastcall EnemyManager::OnDrawImpl(i32 arg1, i32 arg2)
{
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult EnemyManager::OnDrawLowPrio(EnemyManager *enemyManager)
{
    ChainCallbackResult result;

    if (((*(u32 *)0x164D0B4 >> 10) & 1) != 0)
    {
        g_AnmManager->SetMixColor(0xfff01010);
    }
    result = enemyManager->OnDrawImpl(2, 4);
    if (((*(u32 *)0x164D0B4 >> 10) & 1) != 0)
    {
        g_AnmManager->SetMixColorDefault();
    }
    return result;
}

ZunResult EnemyManager::AddedCallback(EnemyManager *enemyManager)
{
    return ZUN_ERROR;
}

ZunResult EnemyManager::DeletedCallback(EnemyManager *enemyManager)
{
    return ZUN_ERROR;
}

void EnemyManager::CutChain()
{
    g_Chain.Cut(&g_EnemyManagerCalcChain);
    g_Chain.Cut(&g_EnemyManagerDrawChainHighPrio);
    g_Chain.Cut(&g_EnemyManagerDrawChainLowPrio);
}

} /* namespace th08 */
