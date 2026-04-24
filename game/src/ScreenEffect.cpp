#include "ScreenEffect.hpp"
#include "AnmManager.hpp"

namespace th08
{

DIFFABLE_STATIC(ScreenEffect, g_ScreenEffect);

ScreenEffect::ScreenEffect()
{
}

void __fastcall ScreenEffect::Clear(D3DCOLOR color)
{
}

void __fastcall ScreenEffect::SetViewport(D3DCOLOR clearColor)
{
    if (g_AnmManager != NULL)
    {
        g_AnmManager->FlushVertexBuffer();
    }

    g_Supervisor.viewport.X = 0;
    g_Supervisor.viewport.Y = 0;
    g_Supervisor.viewport.Width = 640;
    g_Supervisor.viewport.Height = 480;
    g_Supervisor.viewport.MinZ = 0.0f;
    g_Supervisor.viewport.MaxZ = 1.0f;
    g_Supervisor.d3dDevice->SetViewport(&g_Supervisor.viewport);
    Clear(clearColor);
}

ChainCallbackResult __fastcall ScreenEffect::CalcFadeIn(ScreenEffect *screenEffect)
{
    if (screenEffect->fadeDuration != 0)
    {
        screenEffect->fadeAlpha =
            (i32)(255.0f - ((f32)screenEffect->timer * 255.0f) / (f32)screenEffect->fadeDuration);
        if (screenEffect->fadeAlpha < 0)
        {
            screenEffect->fadeAlpha = 0;
        }
    }

    if (screenEffect->timer >= screenEffect->fadeDuration)
    {
        return CHAIN_CALLBACK_RESULT_CONTINUE_AND_REMOVE_JOB;
    }

    screenEffect->timer++;
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}
void __fastcall ScreenEffect::DrawSquare(ZunRect *rectDimensions, D3DCOLOR color)
{
}

void ScreenEffect::DrawSquareShaded(ZunRect *rect, D3DCOLOR topLeft, D3DCOLOR topRight, D3DCOLOR bottomLeft,
                                    D3DCOLOR bottomRight)
{
}

ChainCallbackResult ScreenEffect::CalcFadeOut(ScreenEffect *screenEffect)
{
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ScreenEffect *ScreenEffect::RegisterChain(ScreenEffectType effect, i32 ticks, i32 param_3, i32 param_4, i32 param_5,
                                          i32 param_6)
{
    return NULL;
}

ChainCallbackResult ScreenEffect::DrawFullFade(ScreenEffect *screenEffect)
{
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult __fastcall ScreenEffect::DrawArcadeFade(ScreenEffect *screenEffect)
{
    ZunRect arcadeRect;

    arcadeRect.left = 32.0f;
    arcadeRect.top = 16.0f;
    arcadeRect.right = 416.0f;
    arcadeRect.bottom = 464.0f;
    DrawSquare(&arcadeRect, ((u32)screenEffect->fadeAlpha << 24) | screenEffect->color);

    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult ScreenEffect::CalcShake(ScreenEffect *screenEffect)
{
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ZunResult ScreenEffect::AddedCallback(ScreenEffect *screenEffect)
{
    return ZUN_SUCCESS;
}

ZunResult ScreenEffect::DeletedCallback(ScreenEffect *screenEffect)
{
    return ZUN_SUCCESS;
}

} /* namespace th08 */
