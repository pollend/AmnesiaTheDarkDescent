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
#include <cstddef>
#include <graphics/GraphicsContext.h>
#include <absl/container/fixed_array.h>
#include <graphics/RenderTarget.h>
#include <memory>
#include <vector>
namespace hpl
{

    //------------------------------------------

    class cGraphics;
    class iLowLevelGraphics;
    class iPostEffect;

    //------------------------------------------

    typedef std::multimap<int, iPostEffect*, std::greater<int>> tPostEffectMap;
    typedef tPostEffectMap::iterator tPostEffectMapIt;

    //------------------------------------------

    class cPostEffectComposite : public iRenderFunctions
    {
    public:
        cPostEffectComposite(cGraphics* apGraphics);
        ~cPostEffectComposite();

        // void Render(float afFrameTime, cFrustum* apFrustum, iTexture* apInputTexture, cRenderTarget* apRenderTarget);

		virtual void RebuildSwapChain(uint16_t width, uint16_t height) override;
        void Draw(GraphicsContext& context, Image& inputTexture, RenderTarget& renderTarget);


        /**
         * Highest prio is first!
         */
        void AddPostEffect(iPostEffect* apPostEffect, int alPrio);
        inline int GetPostEffectNum() const
        {
            return _postEffects.size();
        }
        inline iPostEffect* GetPostEffect(int alIdx) const
        {
			for(auto& it: _postEffects) {
				if(it._id == alIdx) {
					return it._effect;
				}
			}
			return nullptr;
        }

        bool HasActiveEffects();

        float GetCurrentFrameTime()
        {
            return mfCurrentFrameTime;
        }

    private:
        struct PostEffectEntry
        {
            size_t _id;
            int _index;
            iPostEffect* _effect;
        };

        void EndRendering();
        void CopyToFrameBuffer(iTexture* apOutputTexture);
        
        std::vector<PostEffectEntry> _postEffects;
        absl::FixedArray<std::shared_ptr<Image>, 2> _images;
        absl::FixedArray<RenderTarget, 2> _renderTargets;

        float mfCurrentFrameTime;
    };

    //------------------------------------------

}; // namespace hpl