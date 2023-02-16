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

#include "engine/Interface.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/Image.h"
#include "graphics/RenderTarget.h"
#include "math/MathTypes.h"
#include "system/LowLevelSystem.h"

#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/PostEffect.h"
#include "graphics/Texture.h"
#include "graphics/VertexBuffer.h"

#include <algorithm>
#include <bx/debug.h>
#include <functional>

namespace hpl {

    cPostEffectComposite::cPostEffectComposite(cGraphics* apGraphics)
        : m_images(2)
        , m_renderTargets(2) {
        mpGraphics = apGraphics;
        SetupRenderFunctions(mpGraphics->GetLowLevel());

        mvRenderTargetSize = mpLowLevelGraphics->GetScreenSizeInt();
        RebuildBuffers();
        m_windowEvent = window::WindowEvent::Handler(
            [&](window::WindowEventPayload& payload) {
                switch (payload.m_type) {
                case hpl::window::WindowEventType::ResizeWindowEvent:
                    {
                        mvRenderTargetSize = cVector2l(payload.payload.m_resizeWindow.m_width, payload.payload.m_resizeWindow.m_height);
                        RebuildBuffers();
                        for(auto& effect : m_postEffects) {
                            effect._effect->OnViewportChanged(mvRenderTargetSize);
                        }
                        break;
                    }
                default:
                    break;
                }
            },
            ConnectionType::QueueConnection);
        if (auto* window = Interface<window::NativeWindowWrapper>::Get()) {
            window->ConnectWindowEventHandler(m_windowEvent);
        }
    }

    cPostEffectComposite::~cPostEffectComposite() {
    }

    void cPostEffectComposite::RebuildBuffers() {
        ImageDescriptor desc;
        desc.m_width = mvRenderTargetSize.x;
        desc.m_height = mvRenderTargetSize.y;
        desc.format = bgfx::TextureFormat::RGBA8;
        desc.m_configuration.m_rt = RTType::RT_Write;

        m_images[0] = std::make_shared<Image>();
        m_images[1] = std::make_shared<Image>();

        m_images[0]->Initialize(desc, nullptr);
        m_images[1]->Initialize(desc, nullptr);

        m_renderTargets[0] = RenderTarget(m_images[0]);
        m_renderTargets[1] = RenderTarget(m_images[1]);
    }

    bool cPostEffectComposite::Draw(GraphicsContext& context, float frameTime, Image& inputTexture, RenderTarget& renderTarget) {
        mfCurrentFrameTime = frameTime;

        auto it = m_postEffects.begin();
        size_t currentIndex = 0;
        bool isSavedToPrimaryRenderTarget = false;
        bool hasEffect = false;
        for (; it != m_postEffects.end(); ++it) {
            if (!it->_effect->IsActive()) {
                continue;
            }
            hasEffect = true;
            it->_effect->RenderEffect(*this, context, inputTexture, m_renderTargets[currentIndex]);
        }

        // this happens when there are no post effects
        if (!hasEffect) {
            cVector2l vRenderTargetSize = GetRenderTargetSize();

            cRect2l rect = cRect2l(0, 0, vRenderTargetSize.x, vRenderTargetSize.y);
            context.CopyTextureToFrameBuffer(inputTexture, rect, renderTarget);
            return false;
        }

        while (it != m_postEffects.end()) {
            if (!it->_effect->IsActive()) {
                it++;
                continue;
            }
            auto nextIt = ([&]() {
                for (; it != m_postEffects.end(); ++it) {
                    if (!it->_effect->IsActive()) {
                        continue;
                    }
                    return it;
                }
                return m_postEffects.end();
            })();
            size_t nextIndex = (currentIndex + 1) % 2;
            if (nextIt == m_postEffects.end()) {
                isSavedToPrimaryRenderTarget = true;
                it->_effect->RenderEffect(*this, context, *m_images[currentIndex], renderTarget);
            } else {
                it->_effect->RenderEffect(*this, context, *m_images[currentIndex], m_renderTargets[nextIndex]);
            }
            currentIndex = nextIndex;
            it = nextIt;
        }
        if (!isSavedToPrimaryRenderTarget) {
            cVector2l vRenderTargetSize = GetRenderTargetSize();

            cRect2l rect = cRect2l(0, 0, vRenderTargetSize.x, vRenderTargetSize.y);
            context.CopyTextureToFrameBuffer(*m_images[currentIndex], rect, renderTarget);
        }
        return true;
    }

    void cPostEffectComposite::AddPostEffect(iPostEffect* apPostEffect, int alPrio) {
        BX_ASSERT(apPostEffect, "Post Effect is not defined");
        if (!apPostEffect) {
            return;
        }
        const auto id = m_postEffects.size();
        m_postEffects.push_back({ id, alPrio, apPostEffect });
        std::sort(m_postEffects.begin(), m_postEffects.end(), [](const auto& a, const auto& b) {
            return a._index > b._index;
        });
    }

    //-----------------------------------------------------------------------

    bool cPostEffectComposite::HasActiveEffects() {
        for (const auto& eff : m_postEffects) {
            if (eff._effect->IsActive()) {
                return true;
            }
        }
        return false;
    }

} // namespace hpl
