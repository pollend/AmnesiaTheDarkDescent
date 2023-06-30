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
#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "scene/Viewport.h"
#include <cstdint>

namespace hpl
{

    class cMaterialType_Translucent_Vars : public iMaterialVars
    {
    public:
        cMaterialType_Translucent_Vars()
            : mbRefraction(false)
            , mbRefractionEdgeCheck(true)
            , mbRefractionNormals(false)
            , mfRefractionScale(0.1f)
            , mfFrenselBias(0.2f)
            , mfFrenselPow(8.0f)
        {
        }
        ~cMaterialType_Translucent_Vars()
        {
        }

        bool mbRefraction;
        bool mbRefractionEdgeCheck;
        bool mbRefractionNormals;
        float mfRefractionScale;
        float mfFrenselBias;
        float mfFrenselPow;
        float mfRimLightMul;
        float mfRimLightPow;
        bool mbAffectedByLightLevel;
    };

    //--------------------------------------------------

    class cMaterialType_Translucent : public iMaterialType
    {
        HPL_RTTI_IMPL_CLASS(iMaterialType, cMaterialType_Translucent, "{cc03a5ac-9137-4e94-ab6b-095a6c9f0085}")

    public:
		cMaterialType_Translucent(cGraphics* apGraphics, cResources* apResources);
        ~cMaterialType_Translucent();

        virtual iMaterialVars* CreateSpecificVariables() override;
        virtual void LoadVariables(cMaterial* apMaterial, cResourceVarsObject* apVars) override;
        virtual void GetVariableValues(cMaterial* apMaterial, cResourceVarsObject* apVars) override;

        virtual void CompileMaterialSpecifics(cMaterial* apMaterial) override;

    private:

        void LoadData();
        void DestroyData();
    };

    //---------------------------------------------------

}; // namespace hpl
