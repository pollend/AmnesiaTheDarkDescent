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

#include "graphics/PostEffect_RadialBlur.h"

#include "bgfx/bgfx.h"
#include "graphics/Graphics.h"

#include "graphics/LowLevelGraphics.h"
#include "graphics/PostEffectComposite.h"
#include "graphics/FrameBuffer.h"
#include "graphics/Texture.h"
#include "graphics/GPUProgram.h"
#include "graphics/GPUShader.h"
#include "graphics/ShaderUtil.h"

#include "math/MathTypes.h"
#include "system/PreprocessParser.h"

namespace hpl {


	cPostEffectType_RadialBlur::cPostEffectType_RadialBlur(cGraphics *apGraphics, cResources *apResources) : iPostEffectType("RadialBlur",apGraphics,apResources)
	{
		// cParserVarContainer vars;
		// mpProgram = mpGraphics->CreateGpuProgramFromShaders("RadialBlur","deferred_base_vtx.glsl", "posteffect_radial_blur_frag.glsl", &vars);
		// if(mpProgram)
		// {
		// 	mpProgram->GetVariableAsId("afSize",kVar_afSize);
		// 	mpProgram->GetVariableAsId("avHalfScreenSize",kVar_avHalfScreenSize);
		// 	mpProgram->GetVariableAsId("afBlurStartDist", kVar_afBlurStartDist);
		// }

		_program = hpl::loadProgram("vs_post_effect", "fs_posteffect_radial_blur_frag");
		_u_uniform = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);
		_s_diffuseMap = bgfx::createUniform("diffuseMap", bgfx::UniformType::Sampler);
	}
	
	cPostEffectType_RadialBlur::~cPostEffectType_RadialBlur()
	{

	}

	iPostEffect * cPostEffectType_RadialBlur::CreatePostEffect(iPostEffectParams *apParams)
	{
		cPostEffect_RadialBlur *pEffect = hplNew(cPostEffect_RadialBlur, (mpGraphics,mpResources,this));
		cPostEffectParams_RadialBlur *pRadialBlurParams = static_cast<cPostEffectParams_RadialBlur*>(apParams);

		return pEffect;
	}

	cPostEffect_RadialBlur::cPostEffect_RadialBlur(cGraphics *apGraphics, cResources *apResources, iPostEffectType *apType) : iPostEffect(apGraphics,apResources,apType)
	{
		cVector2l vSize = mpLowLevelGraphics->GetScreenSizeInt();

		mpRadialBlurType = static_cast<cPostEffectType_RadialBlur*>(mpType);
	}

	//-----------------------------------------------------------------------

	cPostEffect_RadialBlur::~cPostEffect_RadialBlur()
	{

	}

	//-----------------------------------------------------------------------

	void cPostEffect_RadialBlur::Reset()
	{
	}

	//-----------------------------------------------------------------------

	void cPostEffect_RadialBlur::OnSetParams()
	{
	}


	//-----------------------------------------------------------------------

	void cPostEffect_RadialBlur::RenderEffect(GraphicsContext& context, Image& input, RenderTarget& target)
	{
		bgfx::ViewId view = context.StartPass("Radial Blur");

		cVector2l vRenderTargetSize = mpCurrentComposite->GetRenderTargetSize();
		cVector2f vRenderTargetSizeFloat((float)vRenderTargetSize.x, (float)vRenderTargetSize.y);
		if (bgfx::isValid(mpRadialBlurType->_program))
		{
			float value[4] = {  0  };
			value[0] = mParams.mfSize * vRenderTargetSizeFloat.x; // afSize
			value[1] = mParams.mfBlurStartDist; // afBlurStartDist

			cVector2f blurStartDist = vRenderTargetSizeFloat * 0.5f;
			value[2] = blurStartDist.x; // avHalfScreenSize
			value[3] = blurStartDist.y;

			auto& descriptor = target.GetDescriptor();
			bgfx::setViewRect(view, 0, 0, descriptor.width, descriptor.height);
			bgfx::setViewFrameBuffer(view, target.GetHandle());
			bgfx::setTexture(0, mpRadialBlurType->_s_diffuseMap, input.GetHandle());
			bgfx::setUniform(mpRadialBlurType->_u_uniform, &value);
			bgfx::setState(0 | 
				BGFX_STATE_WRITE_RGB | 
				BGFX_STATE_WRITE_A);
			context.ScreenSpaceQuad(target.GetDescriptor().width, target.GetDescriptor().height);
			bgfx::submit(view, mpRadialBlurType->_program);
		}
	}

	iTexture* cPostEffect_RadialBlur::RenderEffect(iTexture *apInputTexture, iFrameBuffer *apFinalTempBuffer)
	{
		/////////////////////////
		// Init render states
		mpCurrentComposite->SetFlatProjection();
		mpCurrentComposite->SetBlendMode(eMaterialBlendMode_None);
		mpCurrentComposite->SetChannelMode(eMaterialChannelMode_RGBA);
		mpCurrentComposite->SetTextureRange(NULL,1);

		cVector2l vRenderTargetSize = mpCurrentComposite->GetRenderTargetSize();
		cVector2f vRenderTargetSizeFloat((float)vRenderTargetSize.x, (float)vRenderTargetSize.y);

		/////////////////////////
		// Render to accum buffer
		// This function sets to frame buffer is post effect is last!
		SetFinalFrameBuffer(apFinalTempBuffer);

		mpCurrentComposite->SetProgram(mpRadialBlurType->mpProgram);

		if(bgfx::isValid(mpRadialBlurType->_program))
		{
			float value[4] = {{0}};
			value[0] = mParams.mfSize*vRenderTargetSizeFloat.x; // afSize
			value[1] = mParams.mfBlurStartDist; // afBlurStartDist
			
			cVector2f blurStartDist = vRenderTargetSizeFloat*0.5f;
			value[2] = blurStartDist.x; // avHalfScreenSize
			value[3] = blurStartDist.y;
			
			bgfx::setUniform(mpRadialBlurType->_u_uniform, &value);

			bgfx::submit(0, mpRadialBlurType->_program);
		}

		mpCurrentComposite->SetTexture(0, apInputTexture);


		DrawQuad(0, 1, apInputTexture, true);

		mpCurrentComposite->SetProgram(NULL);
		mpCurrentComposite->SetBlendMode(eMaterialBlendMode_None);

		return apFinalTempBuffer->GetColorBuffer(0)->ToTexture();
	}

	//-----------------------------------------------------------------------

}
