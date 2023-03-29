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
#pragma once

#include "graphics/Image.h"
#include "graphics/RenderFunctions.h"
#include "math/MathTypes.h"
#include "scene/Viewport.h"
#include <absl/container/fixed_array.h>
#include <array>
#include <graphics/GraphicsContext.h>
#include <graphics/RenderTarget.h>
#include <memory>
#include <vector>
#include <windowing/NativeWindow.h>
namespace hpl {

    //------------------------------------------

    class cGraphics;
    class iLowLevelGraphics;
    class iPostEffect;

    class cPostEffectComposite : public iRenderFunctions {
    public:
        cPostEffectComposite(cGraphics* apGraphics);
        ~cPostEffectComposite();

        struct PostEffectCompositorData {
        public:
        	PostEffectCompositorData() = default;
			PostEffectCompositorData(const PostEffectCompositorData&) = delete;
			PostEffectCompositorData(PostEffectCompositorData&& other):
				m_size(other.m_size),
                m_images(std::move(other.m_images)),
                m_renderTargets(std::move(other.m_renderTargets))
			{}

			PostEffectCompositorData& operator=(const PostEffectCompositorData&) = delete;
			void operator=(PostEffectCompositorData&& other) {
				// m_accumulationBuffer = std::move(buffer.m_accumulationBuffer);
                m_images = std::move(other.m_images);
                m_renderTargets = std::move(other.m_renderTargets);
				m_size = other.m_size;
			}

			cVector2l m_size = cVector2l(0, 0);
            std::array<std::shared_ptr<Image>, 2> m_images;
            std::array<RenderTarget, 2> m_renderTargets;
        };

        // void Render(float afFrameTime, cFrustum* apFrustum, iTexture* apInputTexture, cRenderTarget* apRenderTarget);

        bool Draw(GraphicsContext& context, cViewport& viewport, float frameTime, Image& inputTexture, RenderTarget& renderTarget);

        /**
         * Highest prio is first!
         */
        void AddPostEffect(iPostEffect* apPostEffect, int alPrio);

        inline int GetPostEffectNum() const {
            return m_postEffects.size();
        }

        inline iPostEffect* GetPostEffect(int alIdx) const {
            for (auto& it : m_postEffects) {
                if (it._id == alIdx) {
                    return it._effect;
                }
            }
            return nullptr;
        }

        bool HasActiveEffects();

        float GetCurrentFrameTime() {
            return mfCurrentFrameTime;
        }

    private:
        struct PostEffectEntry {
            size_t _id;
            int _index;
            iPostEffect* _effect;
        };
        
		UniqueViewportData<PostEffectCompositorData> m_boundCompositorData;
        window::WindowEvent::Handler m_windowEvent;
        std::vector<PostEffectEntry> m_postEffects;
        float mfCurrentFrameTime;
    };

}; // namespace hpl
