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

#ifndef HPL_POSTEFFECT_RADIAL_BLUR_H
#define HPL_POSTEFFECT_RADIAL_BLUR_H

#include "graphics/Image.h"
#include <bgfx/bgfx.h>
#include <graphics/PostEffect.h>
#include <graphics/GraphicsContext.h>
#include <graphics/RenderTarget.h>

namespace hpl {

	//------------------------------------------

	class cPostEffectParams_RadialBlur : public iPostEffectParams
	{
	public:
		cPostEffectParams_RadialBlur() : iPostEffectParams("RadialBlur"),
			mfSize(0.06f),
			mfAlpha(1.0f),
			mfBlurStartDist(0)
		{}

		kPostEffectParamsClassInit(cPostEffectParams_RadialBlur)

		float mfSize;
		float mfAlpha;
		float mfBlurStartDist;
	};

	//------------------------------------------

	class cPostEffectType_RadialBlur : public iPostEffectType
	{
	friend class cPostEffect_RadialBlur;
	public:
		cPostEffectType_RadialBlur(cGraphics *apGraphics, cResources *apResources);
		virtual ~cPostEffectType_RadialBlur();

		iPostEffect *CreatePostEffect(iPostEffectParams *apParams);

	private:
		iGpuProgram *mpProgram;

		bgfx::ProgramHandle _program;
		bgfx::UniformHandle _u_uniform;
		bgfx::UniformHandle _s_diffuseMap;
	};

	//------------------------------------------

	class cPostEffect_RadialBlur : public iPostEffect
	{
	public:
		cPostEffect_RadialBlur(cGraphics *apGraphics,cResources *apResources, iPostEffectType *apType);
		~cPostEffect_RadialBlur();

		void Reset();

	private:
		void OnSetParams();
		iPostEffectParams *GetTypeSpecificParams() { return &mParams; }

		void RenderEffect(GraphicsContext& context, Image& input, RenderTarget& target);
		iTexture* RenderEffect(iTexture *apInputTexture, iFrameBuffer *apFinalTempBuffer);

		void RenderBlur(iTexture *apInputTex);

		cPostEffectType_RadialBlur *mpRadialBlurType;
		cPostEffectParams_RadialBlur mParams;

	};

	//------------------------------------------

};
#endif // HPL_POSTEFFECT_IMAGE_TRAIL_H
