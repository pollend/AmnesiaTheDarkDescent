/*
 * Copyright © 2009-2020 Frictional Games
 *
 * This file is part of Amnesia: The Dark Descent.
 *
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "graphics/PostEffectComposite.h"

#include "graphics/GraphicsTypes.h"
#include "graphics/Image.h"
#include "system/LowLevelSystem.h"

#include "graphics/GPUProgram.h"
#include "graphics/GPUShader.h"
#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/PostEffect.h"
#include "graphics/Texture.h"
#include "graphics/VertexBuffer.h"

#include <algorithm>
#include <bx/debug.h>
#include <cstddef>
#include <functional>

namespace hpl
{

    cPostEffectComposite::cPostEffectComposite(cGraphics* apGraphics)
        : _images(2)
        , _renderTargets(2)
    {
        mpGraphics = apGraphics;
        SetupRenderFunctions(mpGraphics->GetLowLevel());

        cVector2l vSize = mpLowLevelGraphics->GetScreenSizeInt();
        for (int i = 0; i < 2; ++i)
        {
            mpFinalTempBuffer[i] = mpGraphics->GetTempFrameBuffer(vSize, ePixelFormat_RGBA, i);
        }
    }

    //-----------------------------------------------------------------------

    cPostEffectComposite::~cPostEffectComposite()
    {
    }


    void cPostEffectComposite::RebuildSwapChain(uint16_t width, uint16_t height)  {
        ImageDescriptor desc;
        desc.width = width;
        desc.height = height;
        desc.format = bgfx::TextureFormat::RGBA8;

        _images[0] = std::make_shared<Image>(desc);
        _images[1] = std::make_shared<Image>(desc);

        _renderTargets[0] = RenderTarget(_images[0]);
        _renderTargets[1] = RenderTarget(_images[1]);
    }

    void cPostEffectComposite::Render(GraphicsContext& context, Image& inputTexture, RenderTarget& renderTarget)
    {
        auto it = _postEffects.begin();
        size_t currentIndex = 0;
        for (; it != _postEffects.end(); ++it)
        {
            if (!it->_effect->IsActive())
            {
                continue;
            }
            it->_effect->RenderEffect(context, inputTexture, _renderTargets[currentIndex]);
        }

        while (it != _postEffects.end())
        {
            if (!it->_effect->IsActive())
            {
                it++;
                continue;
            }
            auto nextIt = ([&](){ 
               for(;it != _postEffects.end(); ++it) {
                   if(!it->_effect->IsActive()) {
                       continue;
                   }
                   return it;
               }
             return _postEffects.end();
            })();
            size_t nextIndex = (currentIndex + 1) % 2;
            if (nextIt == _postEffects.end())
            {
                it->_effect->RenderEffect(context, *_images[currentIndex], renderTarget);
            }
            else
            {
                it->_effect->RenderEffect(context, *_images[currentIndex], _renderTargets[nextIndex]);
            }
            currentIndex = nextIndex;
            it = nextIt;
        }

        // EndRendering();
    }

    void cPostEffectComposite::Render(float afFrameTime, cFrustum* apFrustum, iTexture* apInputTexture, cRenderTarget* apRenderTarget)
    {
        ////////////////////////////////
        // Set up stuff needed for rendering
        BeginRendering(afFrameTime, apFrustum, apInputTexture, apRenderTarget);

        auto lastIt = [&]()
        {
            for (auto it = _postEffects.cend(); it != _postEffects.cbegin();)
            {
                it--;
                if (!it->_effect->IsActive())
                {
                    continue;
                }
                return it;
            }
            return _postEffects.cend();
        }();

        for (auto it = _postEffects.cbegin(); it != _postEffects.cend(); it++)
        {
            if (!it->_effect->IsActive())
            {
                continue;
            }
            if (it == lastIt)
            {
                // it->_effect->Render(afFrameTime, apFrustum, apInputTexture, apRenderTarget);
            }
            else
            {
                // it->_effect->Render(afFrameTime, apFrustum, apInputTexture, _renderTargets[it->_index].get());
            }
        }

        // for(auto& it: m_mapPostEffects) {
        // 	if(!it.second->IsActive()) {
        // 		continue;
        // 	}

        // }

        // ////////////////////////////////
        // //Iterate post effects and find the last one.
        // iPostEffect *pLastEffect = NULL;
        // tPostEffectMapIt it = m_mapPostEffects.begin();
        // for(; it!= m_mapPostEffects.end(); ++it)
        // {
        // 	iPostEffect *pPostEffect = it->second;
        // 	if(pPostEffect->IsActive()==false) continue;

        // 	pLastEffect = pPostEffect;
        // }

        // ////////////////////////////////
        // //Iterate post effects and render them
        // int lCurrentTempBuffer =0;
        // iTexture *pInputTex = apInputTexture;
        // it = m_mapPostEffects.begin();
        // for(; it!= m_mapPostEffects.end(); ++it)
        // {
        // 	iPostEffect *pPostEffect =it->second;
        // 	if(pPostEffect->IsActive()==false) continue;

        // 	bool bLastEffect = pPostEffect == pLastEffect;

        // 	pInputTex = pPostEffect->Render(this,pInputTex,mpFinalTempBuffer[lCurrentTempBuffer] ,bLastEffect);

        // 	lCurrentTempBuffer = lCurrentTempBuffer==0 ? 1 : 0;
        // }

        ///////////////////////////////
        // Reset rendering stuff
        EndRendering();
    }

    //-----------------------------------------------------------------------

    void cPostEffectComposite::AddPostEffect(iPostEffect* apPostEffect, int alPrio)
    {
        BX_ASSERT(apPostEffect, "Post Effect is not defined");
        if (!apPostEffect)
        {
            return;
        }
        _postEffects.push_back({ alPrio, apPostEffect });
        std::sort(
            _postEffects.begin(),
            _postEffects.end(),
            [](const auto& a, const auto& b)
            {
                return a._index > b._index;
            });
    }

    //-----------------------------------------------------------------------

    bool cPostEffectComposite::HasActiveEffects()
    {
        for (const auto& eff : _postEffects)
        {
            if (eff._effect->IsActive())
            {
                return true;
            }
        }
        return false;
    }

    //-----------------------------------------------------------------------

    //////////////////////////////////////////////////////////////////////////
    // PRIVATE METHODS
    //////////////////////////////////////////////////////////////////////////

    //-----------------------------------------------------------------------

    void cPostEffectComposite::BeginRendering(
        float afFrameTime, cFrustum* apFrustum, iTexture* apInputTexture, cRenderTarget* apRenderTarget)
    {
        ///////////////////////////////
        // Init the render functions
        mfCurrentFrameTime = afFrameTime;

        InitAndResetRenderFunctions(apFrustum, apRenderTarget, false);

        ///////////////////////////////
        // Init the render states
        mpLowLevelGraphics->SetColorWriteActive(true, true, true, true);

        mpLowLevelGraphics->SetCullActive(true);
        mpLowLevelGraphics->SetCullMode(eCullMode_CounterClockwise);

        SetDepthTest(false);
        SetDepthWrite(false);
        mpLowLevelGraphics->SetDepthTestFunc(eDepthTestFunc_LessOrEqual);

        mpLowLevelGraphics->SetColor(cColor(1, 1, 1, 1));

        for (int i = 0; i < kMaxTextureUnits; ++i)
            mpLowLevelGraphics->SetTexture(i, NULL);
    }

    //-----------------------------------------------------------------------

    void cPostEffectComposite::EndRendering()
    {
        /////////////////////////////////////////////
        // Reset all rendering states
        SetBlendMode(eMaterialBlendMode_None);
        SetChannelMode(eMaterialChannelMode_RGBA);

        /////////////////////////////////////////////
        // Unbind all rendering data
        for (int i = 0; i < kMaxTextureUnits; ++i)
        {
            if (mvCurrentTexture[i])
                mpLowLevelGraphics->SetTexture(i, NULL);
        }

        if (mpCurrentProgram)
            mpCurrentProgram->UnBind();
        if (mpCurrentVtxBuffer)
            mpCurrentVtxBuffer->UnBind();

        /////////////////////////////////////////////
        // Clean up render functions
        ExitAndCleanUpRenderFunctions();
    }

    //-----------------------------------------------------------------------

    /*void cPostEffectComposite::CopyToFrameBuffer(iTexture *apOutputTexture)
    {
            SetDepthTest(false);
            SetDepthWrite(false);
            SetBlendMode(eMaterialBlendMode_None);
            SetAlphaMode(eMaterialAlphaMode_Solid);
            SetChannelMode(eMaterialChannelMode_RGBA);

            SetFrameBuffer(mpCurrentRenderTarget->mpFrameBuffer,true);

            SetFlatProjection();

            SetProgram(NULL);
            SetTexture(0,apOutputTexture);
            SetTextureRange(NULL, 1);

            ////////////////////////////////////
            //Draw the accumulation buffer to the current frame buffer
            //Since the texture v coordinate is reversed, need to do some math.
            cVector2f vViewportPos((float)mpCurrentRenderTarget->mvPos.x, (float)mpCurrentRenderTarget->mvPos.y);
            cVector2f vViewportSize((float)mvRenderTargetSize.x, (float)mvRenderTargetSize.y);
            DrawQuad(	cVector2f(0,0),1,
                                    cVector2f(vViewportPos.x, (mvScreenSizeFloat.y - vViewportSize.y)-vViewportPos.y ),
                                    cVector2f(vViewportPos.x + vViewportSize.x,mvScreenSizeFloat.y - vViewportPos.y),
                                    true);
    }*/

    //-----------------------------------------------------------------------

} // namespace hpl
