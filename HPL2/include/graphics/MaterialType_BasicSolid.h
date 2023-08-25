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
#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "scene/Viewport.h"
#include <cstdint>

namespace hpl {

    class iMaterialVars;

    namespace material::solid {
        enum DiffuseVariant : uint32_t {
            Diffuse_None = 0,
            Diffuse_NormalMap = 0x00001,
            Diffuse_SpecularMap = 0x00002,
            Diffuse_ParallaxMap = 0x00004,
            Diffuse_EnvMap = 0x00008
        };

        enum ZVariant : uint32_t { Z_None = 0, Z_UseAlphaMap = 0x00001, Z_UseDissolveFilter = 0x00002, Z_UseDissolveAlphaMap = 0x00004 };
    } // namespace material::solid

    class iMaterialType_SolidBase : public iMaterialType {
        HPL_RTTI_IMPL_CLASS(iMaterialType, iMaterialType_SolidBase, "{06ca56f9-381f-4b73-aaa3-3c951b8d6cad}")
    public:

        iMaterialType_SolidBase(cGraphics* apGraphics, cResources* apResources);
        ~iMaterialType_SolidBase();

        void CreateGlobalPrograms();

        iMaterialVars* CreateSpecificVariables() {
            return NULL;
        }
        void LoadVariables(cMaterial* apMaterial, cResourceVarsObject* apVars);
        void GetVariableValues(cMaterial* apMaterial, cResourceVarsObject* apVars);

        void CompileMaterialSpecifics(cMaterial* apMaterial);

    protected:
        virtual void CompileSolidSpecifics(cMaterial* apMaterial) {
        }

        void LoadData();
        void DestroyData();

        Image* m_dissolveImage = nullptr;

        static float mfVirtualPositionAddScale;
    };

    class cMaterialType_SolidDiffuse_Vars : public iMaterialVars {
    public:
        std::array<SharedBuffer, ForgeRenderer::SwapChainLength> m_buffer;

        cMaterialType_SolidDiffuse_Vars()
            : mfHeightMapScale(0.05f)
            , mfHeightMapBias(0.0f)
            , mbAlphaDissolveFilter(false) {
        }
        ~cMaterialType_SolidDiffuse_Vars() {
        }

        struct MaterialBufferData {
            struct {
                mat4 m_uvMtx;
                float alphaReject;
                uint32_t textureConfig;
            } m_z;
        } m_vars;

        uint32_t m_materialId = 0;

        float mfHeightMapScale;
        float mfHeightMapBias;
        float mfFrenselBias;
        float mfFrenselPow;
        bool mbAlphaDissolveFilter;
    };

    class cMaterialType_SolidDiffuse : public iMaterialType_SolidBase {
		HPL_RTTI_IMPL_CLASS(iMaterialType_SolidBase, cMaterialType_SolidDiffuse, "{06904083-3217-48b9-bb27-772df4573557}")
    public:
        static constexpr uint32_t MaxSolidMaterials = 10000;

        cMaterialType_SolidDiffuse(cGraphics* apGraphics, cResources* apResources);
        ~cMaterialType_SolidDiffuse();

        iMaterialVars* CreateSpecificVariables() override;
        void LoadVariables(cMaterial* apMaterial, cResourceVarsObject* apVars) override;
        void GetVariableValues(cMaterial* apMaterial, cResourceVarsObject* apVars) override;

        std::array<SharedDescriptorSet, eMaterialTexture_LastEnum>  m_descriptorSet;
    private:
        uint32_t m_materialCount = 0;
        void CompileSolidSpecifics(cMaterial* apMaterial) override;
    };

    //---------------------------------------------------

}; // namespace hpl
