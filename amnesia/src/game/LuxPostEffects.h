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

#ifndef LUX_POST_EFFECTS_H
#define LUX_POST_EFFECTS_H

//----------------------------------------------

#include "LuxBase.h"
#include "bgfx/bgfx.h"
#include "graphics/Image.h"
#include <bx/debug.h>

class iLuxPostEffect : public iPostEffect
{
public:
	iLuxPostEffect(cGraphics *apGraphics, cResources *apResources) : iPostEffect(apGraphics, apResources, NULL){}
	virtual ~iLuxPostEffect(){}

	virtual void Update(float afTimeStep){}

protected:
	void OnSetParams(){}
	iPostEffectParams *GetTypeSpecificParams() { return NULL; }
};

class cLuxPostEffect_Insanity : public iLuxPostEffect
{
public:
	cLuxPostEffect_Insanity(cGraphics *apGraphics, cResources *apResources);
	~cLuxPostEffect_Insanity();

	void Update(float afTimeStep);

	void SetWaveAlpha(float afX){ mfWaveAlpha = afX;}
	void SetZoomAlpha(float afX){ mfZoomAlpha = afX;}
	void SetWaveSpeed(float afX){ mfWaveSpeed = afX;}
	
	virtual void RenderEffect(cPostEffectComposite& compositor, cViewport& viewport, GraphicsContext& context, Image& input, LegacyRenderTarget& target) override;

private:
	// iGpuProgram *mpProgram;
	// std::vector<iTexture*> mvAmpMaps;
	// iTexture* mpZoomMap;

	bgfx::ProgramHandle m_program;
	std::array<Image*, 3> m_ampMaps;
	Image* m_zoomImage;

	bgfx::UniformHandle m_s_diffuseMap;
	bgfx::UniformHandle m_s_ampMap0;
	bgfx::UniformHandle m_s_ampMap1;
	bgfx::UniformHandle m_s_zoomMap;
	bgfx::UniformHandle m_u_param;

	float mfT = 0.0f;
	float mfAnimCount = 0.0f;
	float mfWaveAlpha = 0.0f;
	float mfZoomAlpha = 0.0f;
	float mfWaveSpeed = 0.0f;
};


//----------------------------------------------


class cLuxPostEffectHandler : public iLuxUpdateable
{
public:
	cLuxPostEffectHandler();
	~cLuxPostEffectHandler();

	void OnStart();
	void Update(float afTimeStep);
	void Reset();

	cLuxPostEffect_Insanity* GetInsanity(){ return mpInsanity; }

private:
	void LoadMainConfig();
	void SaveMainConfig();

	void AddEffect(iLuxPostEffect *apPostEffect, int alPrio);

	cLuxPostEffect_Insanity *mpInsanity;

	std::vector<iLuxPostEffect*> mvPostEffects;

};

//----------------------------------------------


#endif // LUX_POST_EFFECTS_H
