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

#ifndef HPL_POSTEFFECT_IMAGE_TRAIL_H
#define HPL_POSTEFFECT_IMAGE_TRAIL_H

#include "bgfx/bgfx.h"
#include "graphics/ForgeHandles.h"
#include "graphics/PostEffect.h"
#include "scene/Viewport.h"

namespace hpl {

	//------------------------------------------

	class cPostEffectParams_ImageTrail : public iPostEffectParams
	{
	public:
		cPostEffectParams_ImageTrail() : iPostEffectParams("ImageTrail"),

			mfAmount(0.3f)
		{}

		kPostEffectParamsClassInit(cPostEffectParams_ImageTrail)

		float mfAmount;

	};

	class cPostEffectType_ImageTrail : public iPostEffectType
	{
	friend class cPostEffect_ImageTrail;
	public:
	    static constexpr uint32_t DescSetSize = 64;

        cPostEffectType_ImageTrail(cGraphics *apGraphics, cResources *apResources);
		virtual ~cPostEffectType_ImageTrail();

		iPostEffect *CreatePostEffect(iPostEffectParams *apParams);

	private:
        uint32_t m_descIndex = 0;
        Pipeline* m_pipeline = nullptr;
        ForgeShaderHandle m_shader;
        RootSignature* m_rootSignature;
        Sampler* m_inputSampler;
        std::array<DescriptorSet*, ForgeRenderer::SwapChainLength> m_perFrameDescriptorSet;
    };

	class cPostEffect_ImageTrail : public iPostEffect
	{
	public:
		struct ImageTrailData {
		public:
			ImageTrailData() = default;
			ImageTrailData(const ImageTrailData&) = delete;
			ImageTrailData(ImageTrailData&& buffer):
				m_size(buffer.m_size),
				m_accumulationTarget(std::move(buffer.m_accumulationTarget))
			{}

			ImageTrailData& operator=(const ImageTrailData&) = delete;
			void operator=(ImageTrailData&& buffer) {
				m_accumulationTarget = std::move(buffer.m_accumulationTarget);
				m_size = buffer.m_size;
			}
			cVector2l m_size;
			ForgeRenderTarget m_accumulationTarget;
		};

		cPostEffect_ImageTrail(cGraphics *apGraphics,cResources *apResources, iPostEffectType *apType);
		~cPostEffect_ImageTrail();

		virtual void RenderEffect(cPostEffectComposite& compositor, cViewport& viewport, GraphicsContext& context, Image& input, LegacyRenderTarget& target) override;
        virtual void RenderEffect(cPostEffectComposite& compositor, cViewport& viewport, const ForgeRenderer::Frame& frame, Texture* inputTexture, RenderTarget* renderTarget) override;
		virtual void Reset() override;

	private:
		virtual void OnSetActive(bool abX) override;
		virtual void OnSetParams() override;
		iPostEffectParams *GetTypeSpecificParams() override { return &mParams; }

		UniqueViewportData<ImageTrailData> m_boundImageTrailData;

		cPostEffectType_ImageTrail *mpImageTrailType;
		cPostEffectParams_ImageTrail mParams;

		bool mbClearFrameBuffer;
	};

	//------------------------------------------

};
#endif // HPL_POSTEFFECT_IMAGE_TRAIL_H
