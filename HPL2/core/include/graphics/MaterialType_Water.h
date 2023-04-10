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

#include "graphics/MaterialType.h"
#include "graphics/Material.h"
#include "graphics/ShaderVariantCollection.h"
#include <bgfx/bgfx.h>

namespace hpl {


  	namespace material::water
    {
		enum WaterVariant: uint32_t
		{
			None = 0,
			UseReflection = 0x00001,
			UseFog = 0x00002,
			UseCubeMapReflection = 0x00004,
			UseRefraction = 0x00008
		};
	}
	
	class cMaterialType_Water_Vars : public iMaterialVars
	{
	public:
		cMaterialType_Water_Vars() : mbHasReflection(true), mfRefractionScale(0.1f), mfFrenselBias(0.2f), mfFrenselPow(8.0f){}
		~cMaterialType_Water_Vars(){}

        bool mbHasReflection;
		float mfRefractionScale;
		float mfFrenselBias;
		float mfFrenselPow;
		float mfReflectionFadeStart;
		float mfReflectionFadeEnd;
		float mfWaveSpeed;
		float mfWaveAmplitude;
		float mfWaveFreq;
	};

	//-----------------------------------------------------

	class cMaterialType_Water : public iMaterialType
	{
		HPL_RTTI_IMPL_CLASS(iMaterialType, cMaterialType_Decal, "{cc45d7fd-aa4e-4a99-92fa-dc0c8b65bdf3}")
	public:
		cMaterialType_Water(cGraphics *apGraphics, cResources *apResources);
		virtual ~cMaterialType_Water();

		iMaterialVars* CreateSpecificVariables() override;
		void LoadVariables(cMaterial *apMaterial, cResourceVarsObject *apVars) override;
		void GetVariableValues(cMaterial* apMaterial, cResourceVarsObject* apVars) override;

		void CompileMaterialSpecifics(cMaterial *apMaterial) override;

		virtual void ResolveShaderProgram(
            eMaterialRenderMode aRenderMode,
            cViewport& viewport,
            cMaterial* apMaterial,
            iRenderable* apObject,
            iRenderer* apRenderer, 
            std::function<void(GraphicsContext::ShaderProgram&)> handler) override;

	private:
		ShaderVariantCollection<
			material::water::UseReflection|
			material::water::UseFog|
			material::water::UseCubeMapReflection|
			material::water::UseRefraction
		> m_waterVariant;

		bgfx::UniformHandle m_u_param = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_u_mtxInvViewRotation = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_u_fogColor = BGFX_INVALID_HANDLE;

		bgfx::UniformHandle m_s_diffuseMap = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_s_normalMap = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_s_refractionMap = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_s_reflectionMap = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_s_envMap = BGFX_INVALID_HANDLE;

		void LoadData();
		void DestroyData();
	};

	//---------------------------------------------------

};
