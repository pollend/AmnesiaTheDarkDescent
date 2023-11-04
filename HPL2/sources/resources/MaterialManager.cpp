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

#include "resources/MaterialManager.h"
#include "resources/FileSearcher.h"

#include "graphics/Image.h"
#include "graphics/IndexPool.h"
#include "system/LowLevelSystem.h"
#include "system/Platform.h"
#include "system/String.h"
#include "system/System.h"

#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Material.h"
#include "graphics/MaterialType.h"

#include "resources/LowLevelResources.h"
#include "resources/Resources.h"
#include "resources/TextureManager.h"
#include "resources/XmlDocument.h"

#include "impl/tinyXML/tinyxml.h"

#include "Common_3/Utilities/Log/Log.h"
#include "Common_3/Utilities/Interfaces/ILog.h"
#include <FixPreprocessor.h>
#include "tinyxml2.h"

namespace hpl {

    namespace internal {
        static IndexPool m_MaterialIndexPool(cMaterial::MaxMaterialID);
    }

    cMaterialManager::cMaterialManager(cGraphics* apGraphics, cResources* apResources)
        : iResourceManager(apResources->GetFileSearcher(), apResources->GetLowLevel(), apResources->GetLowLevelSystem()) {
        mpGraphics = apGraphics;
        mpResources = apResources;

        mlTextureSizeDownScaleLevel = 0;
        mTextureFilter = eTextureFilter_Bilinear;
        mfTextureAnisotropy = 1.0f;

        mbDisableRenderDataLoading = false;

        mlIdCounter = 0;
    }

    cMaterialManager::~cMaterialManager() {
        DestroyAll();

        LOGF(LogLevel::eINFO," Done with materials\n");
    }

    cMaterial* cMaterialManager::CreateMaterial(const tString& asName) {
        if (asName == "")
            return NULL;

        tWString sPath;
        cMaterial* pMaterial;
        tString asNewName;

        BeginLoad(asName);

        asNewName = cString::SetFileExt(asName, "mat");

        pMaterial = static_cast<cMaterial*>(this->FindLoadedResource(asNewName, sPath));

        if (pMaterial == NULL && sPath != _W("")) {
            pMaterial = LoadFromFile(asNewName, sPath);

            if (pMaterial == NULL) {
                Error("Couldn't load material '%s'\n", asNewName.c_str());
                EndLoad();
                return NULL;
            }

            AddResource(pMaterial);
        }

        if (pMaterial)
            pMaterial->IncUserCount();
        else
            Error("Couldn't create material '%s'\n", asNewName.c_str());

        EndLoad();
        return pMaterial;
    }

    void cMaterialManager::Update(float afTimeStep) {
    }

    void cMaterialManager::Unload(iResourceBase* apResource) {
    }

    void cMaterialManager::Destroy(iResourceBase* apResource) {
        apResource->DecUserCount();

        if (apResource->HasUsers() == false) {
            RemoveResource(apResource);
            hplDelete(apResource);
        }
    }

    void cMaterialManager::SetTextureFilter(eTextureFilter aFilter) {
        if (aFilter == mTextureFilter)
            return;
        mTextureFilter = aFilter;

        tResourceBaseMapIt it = m_mapResources.begin();
        for (; it != m_mapResources.end(); ++it) {
            cMaterial* pMat = static_cast<cMaterial*>(it->second);
            pMat->setTextureFilter(aFilter);
        }
    }

    void cMaterialManager::SetTextureAnisotropy(float afX) {
        if (mfTextureAnisotropy == afX)
            return;
        mfTextureAnisotropy = afX;

        tResourceBaseMapIt it = m_mapResources.begin();
        for (; it != m_mapResources.end(); ++it) {
            cMaterial* pMat = static_cast<cMaterial*>(it->second);
            pMat->SetTextureAnisotropy(afX);
        }
    }

    tString cMaterialManager::GetPhysicsMaterialName(const tString& asName) {
        tWString sPath;
        cMaterial* pMaterial;
        tString asNewName;

        asNewName = cString::SetFileExt(asName, "mat");

        pMaterial = static_cast<cMaterial*>(this->FindLoadedResource(asNewName, sPath));

        if (pMaterial == NULL && sPath != _W("")) {
            FILE* pFile = cPlatform::OpenFile(sPath, _W("rb"));
            if (pFile == NULL)
                return "";

            TiXmlDocument* pDoc = hplNew(TiXmlDocument, ());
            if (!pDoc->LoadFile(pFile)) {
                fclose(pFile);
                hplDelete(pDoc);
                return "";
            }
            fclose(pFile);

            TiXmlElement* pRoot = pDoc->RootElement();

            TiXmlElement* pMain = pRoot->FirstChildElement("Main");
            if (pMain == NULL) {
                hplDelete(pDoc);
                Error("Main child not found in '%s'\n", sPath.c_str());
                return "";
            }

            tString sPhysicsName = cString::ToString(pMain->Attribute("PhysicsMaterial"), "Default");

            hplDelete(pDoc);

            return sPhysicsName;
        }

        if (pMaterial)
            return pMaterial->GetPhysicsMaterial();
        else
            return "";
    }

    cMaterial* cMaterialManager::CreateCustomMaterial(const tString& asName, iMaterialType* apMaterialType) {
        cMaterial* pMat = hplNew(cMaterial, (asName, cString::To16Char(asName), mpResources));
        pMat->IncUserCount();
        AddResource(pMat);
        return pMat;
    }

    cMaterial* cMaterialManager::LoadFromFile(const tString& asName, const tWString& asPath) {
        tinyxml2::XMLDocument document;
		FILE *pFile = cPlatform::OpenFile(asPath, _W("rb"));

        if(!pFile) {
            LOGF(LogLevel::eERROR, "failed to load material: %s", asName.c_str());
            return nullptr;
        }
        document.LoadFile(pFile);
        fclose(pFile);

        auto* rootElement = document.FirstChildElement();
        if (rootElement == nullptr) {
            LOGF(LogLevel::eERROR,"Material-%s: Root not found", asName.c_str());
            return nullptr;
        }
        auto* mainElement = rootElement->FirstChildElement("Main");
        if (mainElement  == nullptr) {
            LOGF(LogLevel::eERROR,"Material-%s: Main child not found", asName.c_str());
            return nullptr;
        }

        const char* sType = "";
        if(mainElement->QueryAttribute("Type", &sType) != tinyxml2::XMLError::XML_SUCCESS) {
            LOGF(LogLevel::eERROR,"Material-%s:Missing Type Attribute: ", asName.c_str());
            return nullptr;
        }

        bool bDepthTest = true;
        float fValue = 1;
        const char* sPhysicsMatName = "Default"; //pMain->GetAttributeString("PhysicsMaterial", "Default");
        const char* sBlendMode = "Add"; //pMain->GetAttributeString("BlendMode", "Add");

        mainElement->QueryBoolAttribute("DepthTest", &bDepthTest);
        mainElement->QueryFloatAttribute("Value", &fValue);
        mainElement->QueryStringAttribute("PhysicsMaterial", &sPhysicsMatName);
        mainElement->QueryStringAttribute("BlendMode", &sBlendMode);

        /////////////////////////////
        // Make a "fake" material, with a blank type
        if (mbDisableRenderDataLoading) {
            cMaterial* pMat = hplNew(cMaterial, (asName, asPath, mpResources));
            pMat->SetPhysicsMaterial(sPhysicsMatName);
            return pMat;
        }

        /////////////////////////////
        // CreateType
        tString normalizedMaterialName = cString::ToLowerCase(sType);
        auto metaInfo = std::find_if(cMaterial::MaterialMetaTable.begin(), cMaterial::MaterialMetaTable.end(), [&](auto& info) {
            return info.m_name == normalizedMaterialName;
        });

        if (metaInfo == cMaterial::MaterialMetaTable.end()) {
            LOGF(eERROR, "Invalid material type %s", sType);
            return NULL;
        }
        cMaterial* pMat = new cMaterial(asName, asPath, mpResources);
        pMat->SetDepthTest(bDepthTest);
        pMat->SetPhysicsMaterial(sPhysicsMatName);

        ///////////////////////////
        // Textures
        auto* textureUnits = rootElement->FirstChildElement("TextureUnits");
        if (textureUnits == nullptr) {
            LOGF(LogLevel::eERROR,"Material-%s: TextureUnits child not found", asName.c_str());
            return NULL;
        }

        for (eMaterialTexture textureType : metaInfo->m_usedTextures) {
            const char* sTextureType = GetTextureString(textureType);
            auto* pTexChild = textureUnits->FirstChildElement(sTextureType);
            if (pTexChild == NULL) {
                continue;
            }
            bool bMipMaps = true;
            bool bCompress = false;
            const char* textureTypeStr = "";
            const char* wrapStr = "";
            const char* sFileQuery = "";
            const char* animModeStr = "None";
            float fFrameTime = 1.0f;

            pTexChild->QueryStringAttribute("AnimMode", &animModeStr);
            pTexChild->QueryStringAttribute("Wrap", &wrapStr);
            pTexChild->QueryStringAttribute("Type", &textureTypeStr);
            pTexChild->QueryStringAttribute("File", &sFileQuery );
            pTexChild->QueryBoolAttribute("MipMaps", &bMipMaps);
            pTexChild->QueryBoolAttribute("Compress", &bCompress);
            pTexChild->QueryFloatAttribute("AnimFrameTime", &fFrameTime);
            eTextureWrap wrap = GetWrap(wrapStr);
            eTextureType type = GetType(textureTypeStr);
            eTextureAnimMode animMode = GetAnimMode(animModeStr);

            if (strcmp(sFileQuery ,"") == 0) {
                continue;
            }
            tString sFile = sFileQuery;
            if (cString::GetFilePath(sFile).length() <= 1) {
                sFile = cString::SetFilePath(sFile, cString::To8Char(cString::GetFilePathW(asPath)));
            }

            cTextureManager::ImageOptions options;
            iResourceBase* pImageResource = nullptr;
            if (animMode != eTextureAnimMode_None) {
                auto animatedImage = mpResources->GetTextureManager()->CreateAnimImage(
                    sFile, bMipMaps, type, eTextureUsage_Normal, mlTextureSizeDownScaleLevel);
                animatedImage->SetFrameTime(fFrameTime);
                animatedImage->SetAnimMode(animMode);
                pMat->SetImage(textureType, animatedImage);
                pImageResource = animatedImage;

            } else {
                Image* pImage = nullptr;
                switch (type) {
                case eTextureType_1D:
                    pImage =
                        mpResources->GetTextureManager()->Create1DImage(sFile, bMipMaps, eTextureUsage_Normal, mlTextureSizeDownScaleLevel);
                    break;
                case eTextureType_2D:
                    pImage = mpResources->GetTextureManager()->Create2DImage(
                        sFile, bMipMaps, eTextureType_2D, eTextureUsage_Normal, mlTextureSizeDownScaleLevel);
                    break;
                case eTextureType_CubeMap:
                    pImage = mpResources->GetTextureManager()->CreateCubeMapImage(
                        sFile, bMipMaps, eTextureUsage_Normal, mlTextureSizeDownScaleLevel);
                    break;
                case eTextureType_3D:
                    pImage =
                        mpResources->GetTextureManager()->Create3DImage(sFile, bMipMaps, eTextureUsage_Normal, mlTextureSizeDownScaleLevel);
                    break;
                default:
                    {
                        ASSERT(false && "Invalid texture type");
                        break;
                    }
                }
                pImageResource = pImage;

                pMat->setTextureWrap(wrap);
                pMat->setTextureFilter(mTextureFilter);
                pMat->SetTextureAnisotropy(mfTextureAnisotropy);
                if (pImage) {
                    pMat->SetImage(textureType, pImage);
                }
            }
            if (!pImageResource) {
                hplDelete(pMat);
                return nullptr;
            }
        }
        ///////////////////////////
        // Animations
        auto* pUvAnimRoot  = rootElement->FirstChildElement("UvAnimations");
        if (pUvAnimRoot) {
            auto animElement = pUvAnimRoot->FirstChildElement();
            for(;animElement != nullptr; animElement = animElement->NextSiblingElement()) {
                const char* animTypeStr = "";
                const char* animAxisStr = "";
                float fSpeed = 0;
                float fAmp = 0;
                auto* animationNode = animElement->ToElement();
                animationNode->QueryStringAttribute("Type", &animTypeStr);
                animationNode->QueryStringAttribute("Axis", &animAxisStr);

                eMaterialUvAnimation animType = GetUvAnimType(animTypeStr);
                eMaterialAnimationAxis animAxis = GetAnimAxis(animAxisStr);
                animationNode->QueryFloatAttribute("Speed", &fSpeed);
                animationNode->QueryFloatAttribute("Amplitude", &fAmp);
                pMat->AddUvAnimation(animType, fSpeed, fAmp, animAxis);
            }
        }

        ///////////////////////////
        // Variables
        cResourceVarsObject userVars;
        auto* pUserVarsRoot = rootElement->FirstChildElement("SpecificVariables");
        if (pUserVarsRoot)
            userVars.LoadVariables(pUserVarsRoot);

        for (auto& meta : cMaterial::MaterialMetaTable) {
            if (normalizedMaterialName == meta.m_name) {
                pMat->SetHandle(IndexPoolHandle(&internal::m_MaterialIndexPool));
                ShaderMaterialData materialDescriptor;
                materialDescriptor.m_id = meta.m_id;
                switch (meta.m_id) {
                case MaterialID::SolidDiffuse:
                    {
                        materialDescriptor.m_solid.m_heightMapScale = userVars.GetVarFloat("HeightMapScale", 0.1f);
                        materialDescriptor.m_solid.m_heightMapBias = userVars.GetVarFloat("HeightMapBias", 0);
                        materialDescriptor.m_solid.m_frenselBias = userVars.GetVarFloat("FrenselBias", 0.2f);
                        materialDescriptor.m_solid.m_frenselPow = userVars.GetVarFloat("FrenselPow", 8.0f);
                        materialDescriptor.m_solid.m_alphaDissolveFilter = userVars.GetVarBool("AlphaDissolveFilter", false);
                        break;
                    }
                case MaterialID::Translucent:
                    {
                        materialDescriptor.m_translucent.m_hasRefraction = userVars.GetVarBool("Refraction", false);
                        materialDescriptor.m_translucent.m_refractionNormals = userVars.GetVarBool("RefractionNormals", true);
                        materialDescriptor.m_translucent.m_refractionEdgeCheck = userVars.GetVarBool("RefractionEdgeCheck", true);
                        materialDescriptor.m_translucent.m_isAffectedByLightLevel = userVars.GetVarBool("AffectedByLightLevel", false);

                        materialDescriptor.m_translucent.m_refractionScale = userVars.GetVarFloat("RefractionScale", 1.0f);
                        materialDescriptor.m_translucent.m_frenselBias = userVars.GetVarFloat("FrenselBias", 0.2f);
                        materialDescriptor.m_translucent.m_frenselPow = userVars.GetVarFloat("FrenselPow", 8.0);
                        materialDescriptor.m_translucent.m_rimLightMul = userVars.GetVarFloat("RimLightMul", 0.0f);
                        materialDescriptor.m_translucent.m_rimLightPow = userVars.GetVarFloat("RimLightPow", 8.0f);
                        materialDescriptor.m_translucent.m_blend = GetBlendMode(sBlendMode);
                        break;
                    }
                case MaterialID::Water:
                    {
                        materialDescriptor.m_water.m_hasReflection = userVars.GetVarBool("HasReflection", true);
                        materialDescriptor.m_water.m_refractionScale = userVars.GetVarFloat("RefractionScale", 1.0f);
                        materialDescriptor.m_water.m_frenselBias = userVars.GetVarFloat("FrenselBias", 0.2f);
                        materialDescriptor.m_water.m_frenselPow = userVars.GetVarFloat("FrenselPow", 8.0f);
                        materialDescriptor.m_water.m_reflectionFadeStart = userVars.GetVarFloat("ReflectionFadeStart", 0);
                        materialDescriptor.m_water.m_reflectionFadeEnd = userVars.GetVarFloat("ReflectionFadeEnd", 0);
                        materialDescriptor.m_water.m_waveSpeed = userVars.GetVarFloat("WaveSpeed", 1.0f);
                        materialDescriptor.m_water.m_waveAmplitude = userVars.GetVarFloat("WaveAmplitude", 1.0f);
                        materialDescriptor.m_water.m_waveFreq = userVars.GetVarFloat("WaveFreq", 1.0f);

                        materialDescriptor.m_water.m_isLargeSurface = userVars.GetVarBool("LargeSurface", false);
                        materialDescriptor.m_water.m_worldReflectionOcclusionTest =
                            userVars.GetVarBool("OcclusionCullWorldReflection", true);
                        break;
                    }
                case MaterialID::Decal:
                    {
                        materialDescriptor.m_translucent.m_blend = GetBlendMode(sBlendMode);
                        break;
                    }
                default:
                    ASSERT(false && "Invalid material type");
                    break;
                }
                pMat->SetDescriptor(materialDescriptor);
                break;
            }
        }
        return pMat;
    }

    eTextureType cMaterialManager::GetType(const char* asType) {
        if (stricmp(asType, "cube") == 0) {
            return eTextureType_CubeMap;
        } else if (stricmp(asType, "1d")== 0) {
            return eTextureType_1D;
        } else if (stricmp(asType, "2d")== 0) {
            return eTextureType_2D;
        } else if (stricmp(asType, "3d")== 0) {
            return eTextureType_3D;
        }
        return eTextureType_2D;
    }

    const char* cMaterialManager::GetTextureString(eMaterialTexture aType) {
        switch (aType) {
        case eMaterialTexture_Diffuse:
            return "Diffuse";
        case eMaterialTexture_Alpha:
            return "Alpha";
        case eMaterialTexture_NMap:
            return "NMap";
        case eMaterialTexture_Height:
            return "Height";
        case eMaterialTexture_Illumination:
            return "Illumination";
        case eMaterialTexture_Specular:
            return "Specular";
        case eMaterialTexture_CubeMap:
            return "CubeMap";
        case eMaterialTexture_DissolveAlpha:
            return "DissolveAlpha";
        case eMaterialTexture_CubeMapAlpha:
            return "CubeMapAlpha";
        default:
            break;
        }
        return "";
    }

    //-----------------------------------------------------------------------

    eTextureWrap cMaterialManager::GetWrap(const char* asType) {
        if (stricmp(asType, "repeat")== 0) {
            return eTextureWrap_Repeat;
        } else if (stricmp(asType, "clamp")== 0) {
            return eTextureWrap_Clamp;
        } else if (stricmp(asType, "clamptoedge")== 0) {
            return eTextureWrap_ClampToEdge;
        }
        return eTextureWrap_Repeat;
    }

    eTextureAnimMode cMaterialManager::GetAnimMode(const char* asType) {
        if (stricmp(asType, "none")== 0) {
            return eTextureAnimMode_None;
        } else if (stricmp(asType, "loop")== 0) {
            return eTextureAnimMode_Loop;
        } else if (stricmp(asType, "oscillate")== 0) {
            return eTextureAnimMode_Oscillate;
        }
        return eTextureAnimMode_None;
    }

    //-----------------------------------------------------------------------

    eMaterialBlendMode cMaterialManager::GetBlendMode(const char* asType) {
        if (stricmp(asType, "add")== 0) {
            return eMaterialBlendMode_Add;
        }
        if (stricmp(asType, "mul")== 0) {
            return eMaterialBlendMode_Mul;
        }
        if (stricmp(asType, "mulx2")== 0) {
            return eMaterialBlendMode_MulX2;
        }
        if (stricmp(asType, "alpha")== 0) {
            return eMaterialBlendMode_Alpha;
        }
        if (stricmp(asType, "premulalpha")== 0) {
            return eMaterialBlendMode_PremulAlpha;
        }

        LOGF(LogLevel::eERROR, "Material BlendMode '%s' does not exist!", asType);
        return eMaterialBlendMode_Add;
    }

    //-----------------------------------------------------------------------

    eMaterialUvAnimation cMaterialManager::GetUvAnimType(const char* apString) {
        if (apString == NULL) {
            Error("Uv animation attribute Type does not exist!\n");
            return eMaterialUvAnimation_LastEnum;
        }

        tString sLow = cString::ToLowerCase(apString);

        if (sLow == "translate") {
            return eMaterialUvAnimation_Translate;
        }
        if (sLow == "sin") {
            return eMaterialUvAnimation_Sin;
        }
        if (sLow == "rotate") {
            return eMaterialUvAnimation_Rotate;
        }

        LOGF(LogLevel::eERROR, "Invalid uv animation type %s", apString);
        return eMaterialUvAnimation_LastEnum;
    }

    eMaterialAnimationAxis cMaterialManager::GetAnimAxis(const char* apString) {
        if (apString == NULL) {
            Error("Uv animation attribute Axis does not exist!\n");
            return eMaterialAnimationAxis_LastEnum;
        }

        tString sLow = cString::ToLowerCase(apString);

        if (sLow == "x") {
            return eMaterialAnimationAxis_X;
        }
        if (sLow == "y") {
            return eMaterialAnimationAxis_Y;
        }
        if (sLow == "z") {
            return eMaterialAnimationAxis_Z;
        }
        LOGF(LogLevel::eERROR, "Invalid animation axis %s", apString);
        return eMaterialAnimationAxis_LastEnum;
    }

} // namespace hpl
