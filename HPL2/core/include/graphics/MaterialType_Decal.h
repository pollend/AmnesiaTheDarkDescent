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

#include "bgfx/bgfx.h"
#include "graphics/MaterialType.h"
#include "graphics/Material.h"

namespace hpl {

	//---------------------------------------------------
	// Decal
	//---------------------------------------------------

	class cMaterialType_Decal_Vars : public iMaterialVars
	{
	public:
		cMaterialType_Decal_Vars(){}
		~cMaterialType_Decal_Vars(){}
   	};

	//-----------------------------------------------------

	class cMaterialType_Decal : public iMaterialType
	{
	public:
		cMaterialType_Decal(cGraphics *apGraphics, cResources *apResources);
		~cMaterialType_Decal();

		iMaterialVars* CreateSpecificVariables();
		void LoadVariables(cMaterial *apMaterial, cResourceVarsObject *apVars);
		void GetVariableValues(cMaterial* apMaterial, cResourceVarsObject* apVars);

		void CompileMaterialSpecifics(cMaterial *apMaterial);

		virtual void ResolveShaderProgram(
            eMaterialRenderMode aRenderMode,
            cMaterial* apMaterial,
            iRenderable* apObject,
            iRenderer* apRenderer, 
            std::function<void(GraphicsContext::ShaderProgram&)> handler) override;



	private:
		bgfx::ProgramHandle m_programHandler;
		bgfx::UniformHandle m_u_mtxUV;
		bgfx::UniformHandle m_s_diffuseMap;

		void LoadData();
		void DestroyData();
	};

	//---------------------------------------------------

};
