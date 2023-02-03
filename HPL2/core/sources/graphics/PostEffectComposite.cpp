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
#include "graphics/RenderTarget.h"
#include "math/MathTypes.h"
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
        
        ImageDescriptor desc;
        desc.m_width = vSize.x;
        desc.m_height = vSize.y;
        desc.format = bgfx::TextureFormat::RGBA8;
        desc.m_configuration.m_rt = RTType::RT_Write; 

        _images[0] = std::make_shared<Image>();
        _images[1] = std::make_shared<Image>();

        _images[0]->Initialize(desc, nullptr);
        _images[1]->Initialize(desc, nullptr);

        _renderTargets[0] = RenderTarget(_images[0]);
        _renderTargets[1] = RenderTarget(_images[1]);

        mvRenderTargetSize.x = vSize.x;
        mvRenderTargetSize.y = vSize.y;
    }

    //-----------------------------------------------------------------------

    cPostEffectComposite::~cPostEffectComposite()
    {
    }



    bool cPostEffectComposite::Draw(GraphicsContext& context, Image& inputTexture, RenderTarget& renderTarget)
    {
        auto it = _postEffects.begin();
        size_t currentIndex = 0;
        bool isSavedToPrimaryRenderTarget = false;
        bool hasEffect = false;
        for (; it != _postEffects.end(); ++it)
        {
            if (!it->_effect->IsActive())
            {
                continue;
            }
            hasEffect = true;
            it->_effect->RenderEffect(*this, context, inputTexture, _renderTargets[currentIndex]);
        }

        // this happens when there are no post effects
        if(!hasEffect) {
            cVector2l vRenderTargetSize = GetRenderTargetSize();

            cRect2l rect = cRect2l(0, 0, vRenderTargetSize.x, vRenderTargetSize.y);
            context.CopyTextureToFrameBuffer(inputTexture, rect, renderTarget);
            return false;
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
                isSavedToPrimaryRenderTarget = true;
                it->_effect->RenderEffect(*this, context, *_images[currentIndex], renderTarget);
            }
            else
            {
                it->_effect->RenderEffect(*this, context, *_images[currentIndex], _renderTargets[nextIndex]);
            }
            currentIndex = nextIndex;
            it = nextIt;
        }
        if(!isSavedToPrimaryRenderTarget) {
            cVector2l vRenderTargetSize = GetRenderTargetSize();

            cRect2l rect = cRect2l(0, 0, vRenderTargetSize.x, vRenderTargetSize.y);
            context.CopyTextureToFrameBuffer(*_images[currentIndex], rect, renderTarget);
        }
        return true;

    }

    void cPostEffectComposite::AddPostEffect(iPostEffect* apPostEffect, int alPrio)
    {
        BX_ASSERT(apPostEffect, "Post Effect is not defined");
        if (!apPostEffect)
        {
            return;
        }
        const auto id = _postEffects.size();
        _postEffects.push_back({ id ,alPrio, apPostEffect });
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

    void cPostEffectComposite::EndRendering()
    {
        /////////////////////////////////////////////
        // Reset all rendering states
        SetBlendMode(eMaterialBlendMode_None);
        SetChannelMode(eMaterialChannelMode_RGBA);

        if (mpCurrentProgram)
            mpCurrentProgram->UnBind();
        if (mpCurrentVtxBuffer)
            mpCurrentVtxBuffer->UnBind();

        /////////////////////////////////////////////
        // Clean up render functions
        ExitAndCleanUpRenderFunctions();
    }

} // namespace hpl
