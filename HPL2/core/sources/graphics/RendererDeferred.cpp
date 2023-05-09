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

#include "graphics/RendererDeferred.h"

#include "Common_3/Utilities/Interfaces/ILog.h"
#include "bgfx/bgfx.h"
#include "engine/Event.h"
#include "engine/Interface.h"
#include "graphics/ImmediateDrawBatch.h"
#include "scene/Viewport.h"
#include "windowing/NativeWindow.h"
#include <bx/debug.h>

#include <cstdint>
#include <graphics/Enum.h>

#include "bx/math.h"

#include "graphics/GraphicsContext.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/Image.h"
#include "graphics/RenderTarget.h"
#include "impl/LegacyVertexBuffer.h"
#include "math/Math.h"

#include "math/MathTypes.h"
#include "scene/SceneTypes.h"
#include "system/LowLevelSystem.h"
#include "system/PreprocessParser.h"
#include "system/String.h"

#include "graphics/FrameBuffer.h"
#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Material.h"
#include "graphics/MaterialType.h"
#include "graphics/Mesh.h"
#include "graphics/RenderList.h"
#include "graphics/Renderable.h"
#include "graphics/ShaderUtil.h"
#include "graphics/SubMesh.h"
#include "graphics/Texture.h"
#include "graphics/TextureCreator.h"
#include "graphics/VertexBuffer.h"

#include "graphics/GPUShader.h"
#include "resources/MeshManager.h"
#include "resources/Resources.h"
#include "resources/TextureManager.h"

#include "scene/Camera.h"
#include "scene/FogArea.h"
#include "scene/Light.h"
#include "scene/LightBox.h"
#include "scene/LightSpot.h"
#include "scene/MeshEntity.h"
#include "scene/RenderableContainer.h"
#include "scene/World.h"

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>

namespace hpl {

    bool cRendererDeferred::mbDepthCullLights = true;

    bool cRendererDeferred::mbSSAOLoaded = false;
    int cRendererDeferred::mlSSAONumOfSamples = 8;
    int cRendererDeferred::mlSSAOBufferSizeDiv = 2;
    float cRendererDeferred::mfSSAOScatterLengthMul = 0.2f;
    float cRendererDeferred::mfSSAOScatterLengthMin = 0.015f;
    float cRendererDeferred::mfSSAOScatterLengthMax = 0.13f;
    float cRendererDeferred::mfSSAODepthDiffMul = 1.5f;
    float cRendererDeferred::mfSSAOSkipEdgeLimit = 3.0f;
    eDeferredSSAO cRendererDeferred::mSSAOType = eDeferredSSAO_OnColorBuffer;
    bool cRendererDeferred::mbEdgeSmoothLoaded = false;

    static constexpr float kLightRadiusMul_High = 1.08f;
    static constexpr float kLightRadiusMul_Medium = 1.12f;
    static constexpr float kLightRadiusMul_Low = 1.2f;
    static constexpr uint32_t kMaxStencilBitsUsed = 8;
    static constexpr uint32_t kStartStencilBit = 0;
    static constexpr uint32_t MinLargeLightNormalizedArea = 0.2f * 0.2f;

    static constexpr uint32_t SSAOImageSizeDiv = 2;
    static constexpr uint32_t SSAONumOfSamples = 8;

    namespace detail {
        struct MaterialInfo {
            void* m_material = nullptr; // void* to avoid accessing the material 
            uint32_t m_version = 0; // version of the material
            absl::InlinedVector<void*, 10> m_visitedDescriptors; // list of descriptors that have been visited
        };
        // TODO: move back into cRendererDeferred
        static ForgeBufferHandle MaterialUniformBuffer;
        static std::array<MaterialInfo, cMaterial::MaxMaterialID> m_materialInfo;
        static GPURingBuffer* CObjectRingBuffer;

        struct PerObjectOption {
            cMatrixf m_viewMat;
            cMatrixf m_projectionMat;
        };

        void cmdBindObjectDescriptor(const ForgeRenderer::Frame& frame, uint32_t& updateIndex, cFrustum* frustum, cMaterial* apMaterial, iRenderable* apObject, DescriptorSet* apDescriptorSet, const PerObjectOption& option) {
		    GPURingBufferOffset uniformBuffer = getGPURingBufferOffset(CObjectRingBuffer, sizeof(cRendererDeferred::CBObjectData));
            
            cRendererDeferred::CBObjectData uniformObjectData = {};
            cMatrixf modelMat = apObject->GetModelMatrixPtr() ? *apObject->GetModelMatrixPtr() : cMatrixf::Identity;
            cMatrixf modelViewMat = cMath::MatrixMul(option.m_viewMat, modelMat);
            cMatrixf modelViewProjMat = cMath::MatrixMul(cMath::MatrixMul(option.m_projectionMat, option.m_viewMat), modelMat);

            uniformObjectData.m_modelMat = cMath::ToForgeMat4(modelMat.GetTranspose());
            if(apMaterial) {
                uniformObjectData.m_uvMat = cMath::ToForgeMat4(apMaterial->GetUvMatrix());
            }
            uniformObjectData.m_modelViewMat =  cMath::ToForgeMat4(modelViewMat.GetTranspose());
            uniformObjectData.m_modelViewProjMat = cMath::ToForgeMat4(modelViewProjMat.GetTranspose());

            // uniformObjectData.m_normalMatrix = cMath::ToForgeMat3(apObject->GetNormalMatrixPtr() ? *apObject->GetNormalMatrixPtr() : cMatrixf::Identity);

            BufferUpdateDesc updateDesc = { uniformBuffer.pBuffer, uniformBuffer.mOffset };
			beginUpdateResource(&updateDesc);
			(*reinterpret_cast<cRendererDeferred::CBObjectData*>(updateDesc.pMappedData)) = uniformObjectData;
			endUpdateResource(&updateDesc, NULL);
			
            DescriptorData params[15] = {};
            size_t paramCount = 0;
            params[paramCount].pName = "uniformObjectBlock";
            DescriptorDataRange range = { (uint32_t)uniformBuffer.mOffset, sizeof(cRendererDeferred::CBObjectData) };
            params[paramCount].pRanges = &range;
            params[paramCount++].ppBuffers = &uniformBuffer.pBuffer;

            updateDescriptorSet(frame.m_renderer->Rend(), updateIndex, apDescriptorSet, paramCount, params);
            cmdBindDescriptorSet(frame.m_cmd, updateIndex, apDescriptorSet);
            updateIndex = (updateIndex + 1) % cRendererDeferred::MaxObjectUniforms;
        }

        void cmdDefaultLegacyGeomBinding(const ForgeRenderer::Frame& frame, LegacyVertexBuffer::GeometryBinding& binding) {
            absl::InlinedVector<Buffer*, 16> vbBuffer;
            absl::InlinedVector<uint64_t, 16> vbOffsets;
            absl::InlinedVector<uint32_t, 16> vbStride;

            for (auto& element : binding.m_vertexElement) {
                vbBuffer.push_back(element.element->m_buffer.m_handle);
                vbOffsets.push_back(element.offset);
                vbStride.push_back(element.element->Stride());
                frame.m_resourcePool->Push(element.element->m_buffer);
            }
            frame.m_resourcePool->Push(*binding.m_indexBuffer.element);
            
            cmdBindVertexBuffer(frame.m_cmd, binding.m_vertexElement.size(), vbBuffer.data(), vbStride.data(), vbOffsets.data());
            cmdBindIndexBuffer(
                frame.m_cmd, binding.m_indexBuffer.element->m_handle, INDEX_TYPE_UINT32, binding.m_indexBuffer.offset);
        }

        void cmdBindMaterialDescriptor(const ForgeRenderer::Frame& frame, cMaterial* apMaterial, std::span<const eMaterialTexture> materials, DescriptorSet* apDescriptorSet) {
            auto& materialType = apMaterial->type();
            auto& info = m_materialInfo[apMaterial->materialID()];
            // auto& metaInfo = cMaterial::MetaInfo[materialType.m_id];

            // we are using the same material, but the version has changed so we need to update the descriptor
            // associated with the material
            const bool isVisited = std::find_if(info.m_visitedDescriptors.begin(), info.m_visitedDescriptors.end(), [&](const auto& desc) {
                                       return desc == apDescriptorSet;
                                   }) != info.m_visitedDescriptors.end();
            const bool isDirty = (info.m_material != apMaterial || info.m_version != apMaterial->Version());

            if (isDirty || !isVisited) {
                if (isDirty) {
                    info.m_version = apMaterial->Version();
                    info.m_material = apMaterial;
                    info.m_visitedDescriptors.clear();

                    BufferUpdateDesc  updateDesc = { MaterialUniformBuffer.m_handle, apMaterial->materialID() * sizeof(cMaterial::MaterialType::MaterialData) };
                    beginUpdateResource(&updateDesc);
                    memcpy(updateDesc.pMappedData, &materialType.m_data, sizeof(cMaterial::MaterialType::MaterialData));
                    endUpdateResource(&updateDesc, NULL);
                }

                DescriptorData params[15] = {};
                size_t paramCount = 0;
 
                for (auto& supportedTexture : materials) {
                    static constexpr const char* TextureNameLookup[] = {
                        "diffuseMap", // eMaterialTexture_Diffuse
                        "normalMap", // eMaterialTexture_NMap
                        "specularMap", // eMaterialTexture_Specular
                        "alphaMap", // eMaterialTexture_Alpha
                        "heightMap", // eMaterialTexture_Height
                        "illuminationMap", // eMaterialTexture_Illumination
                        "cubeMap", // eMaterialTexture_CubeMap
                        "dissolveAlphaMap", // eMaterialTexture_DissolveAlpha
                        "cubeMapAlpha", // eMaterialTexture_CubeMapAlpha
                    };

                    auto* image = apMaterial->GetImage(supportedTexture);
                    if (image) {
                        params[paramCount].pName = TextureNameLookup[supportedTexture];
                        params[paramCount++].ppTextures = &image->GetTexture().m_handle;
                    }
                }

                DescriptorDataRange range = { static_cast<uint32_t>(apMaterial->materialID() * sizeof(cMaterial::MaterialType::MaterialData)), sizeof(cMaterial::MaterialType::MaterialData) };
                params[paramCount].pName = "uniformMaterialBlock";
                params[paramCount].pRanges = &range;
                params[paramCount++].ppBuffers = &MaterialUniformBuffer.m_handle;

                updateDescriptorSet(
                    frame.m_renderer->Rend(), apMaterial->materialID(), apDescriptorSet, paramCount, params);
            }
            cmdBindDescriptorSet(frame.m_cmd, apMaterial->materialID(), apDescriptorSet);

        }

        struct DeferredLight {
        public:
            DeferredLight() = default;
            cRect2l m_clipRect;
            cMatrixf m_mtxViewSpaceRender;
            cMatrixf m_mtxViewSpaceTransform;
            eShadowMapResolution m_shadowResolution = eShadowMapResolution_Low;
            iLight* m_light = nullptr;
            bool m_insideNearPlane = false;
            bool m_castShadows = false;

            inline float getArea() {
                return m_clipRect.w * m_clipRect.h;
            }
        };

        static inline std::vector<cRendererDeferred::FogRendererData> createFogRenderData(std::span<cFogArea*> fogAreas, cFrustum* apFrustum) {
            std::vector<cRendererDeferred::FogRendererData> fogRenderData;
            fogRenderData.reserve(fogAreas.size());
            for (const auto& fogArea : fogAreas) {
                auto& fogData =
                    fogRenderData.emplace_back(cRendererDeferred::FogRendererData{ fogArea, false, cVector3f(0.0f), cMatrixf(cMatrixf::Identity) });
                fogData.m_fogArea = fogArea;
                fogData.m_mtxInvBoxSpace = cMath::MatrixInverse(*fogArea->GetModelMatrixPtr());
                fogData.m_boxSpaceFrustumOrigin = cMath::MatrixMul(fogData.m_mtxInvBoxSpace, apFrustum->GetOrigin());
                fogData.m_insideNearFrustum = ([&]() {
                    std::array<cVector3f, 4> nearPlaneVtx;
                    cVector3f min(-0.5f);
                    cVector3f max(0.5f);

                    for (size_t i = 0; i < nearPlaneVtx.size(); ++i) {
                        nearPlaneVtx[i] = cMath::MatrixMul(fogData.m_mtxInvBoxSpace, apFrustum->GetVertex(i));
                    }
                    for (size_t i = 0; i < nearPlaneVtx.size(); ++i) {
                        if (cMath::CheckPointInAABBIntersection(nearPlaneVtx[i], min, max)) {
                            return true;
                        }
                    }
                    //////////////////////////////
                    // Check if near plane points intersect with box
                    if (cMath::CheckPointsAABBPlanesCollision(nearPlaneVtx.data(), 4, min, max) != eCollision_Outside) {
                        return true;
                    }
                    return false;
                })();
            }
            return fogRenderData;
        }


        static inline bool SetupShadowMapRendering(std::vector<iRenderable*>& shadowCasters, cWorld* world, cFrustum* frustum,  iLight* light, std::span<cPlanef> clipPlanes) {
            /////////////////////////
            // Get light data
            if (light->GetLightType() != eLightType_Spot)
                return false; // Only support spot lights for now...

            cLightSpot* pSpotLight = static_cast<cLightSpot*>(light);
            cFrustum* pLightFrustum = pSpotLight->GetFrustum();

            std::function<void(iRenderableContainerNode* apNode, eCollision aPrevCollision)> walkShadowCasters;
            walkShadowCasters = [&](iRenderableContainerNode* apNode, eCollision aPrevCollision) {

                    ///////////////////////////////////////
                // Get frustum collision, if previous was inside, then this is too!
                eCollision frustumCollision = aPrevCollision == eCollision_Inside ? aPrevCollision : pLightFrustum->CollideNode(apNode);

                ///////////////////////////////////
                // Check if visible but always iterate the root node!
                if (apNode->GetParent()) {
                    if (frustumCollision == eCollision_Outside) {
                        return;
                    }
                    if (rendering::detail::IsRenderableNodeIsVisible(apNode, clipPlanes) == false) {
                        return;
                    }
                }

                ////////////////////////
                // Iterate children
                if (apNode->HasChildNodes()) {
                    for (auto& childNode : apNode->GetChildNodes()) {
                        walkShadowCasters(childNode, frustumCollision);
                    }
                }

                /////////////////////////////
                // Iterate objects
                if (apNode->HasObjects()) {
                    for (auto& object : apNode->GetObjects()) {
                        // Check so visible and shadow caster
                        if (rendering::detail::IsObjectIsVisible(object, eRenderableFlag_ShadowCaster, clipPlanes) == false || object->GetMaterial() == NULL ||
                            object->GetMaterial()->GetType()->IsTranslucent()) {
                            continue;
                        }

                        /////////
                        // Check if in frustum
                        if (frustumCollision != eCollision_Inside &&
                            frustum->CollideBoundingVolume(object->GetBoundingVolume()) == eCollision_Outside) {
                            continue;
                        }

                        // Calculate the view space Z (just a squared distance)
                        object->SetViewSpaceZ(cMath::Vector3DistSqr(object->GetBoundingVolume()->GetWorldCenter(), frustum->GetOrigin()));

                        // Add to list
                        shadowCasters.push_back(object);
                    }
                }
            };


            /////////////////////////
            // If culling by occlusion, skip rest of function
            if (light->GetOcclusionCullShadowCasters())
            {
                return true;
            }

            /////////////////////////
            // Get objects to render

            // Clear list
            shadowCasters.resize(0); // No clear, so we keep all in memory.

            // Get the objects
            if (light->GetShadowCastersAffected() & eObjectVariabilityFlag_Dynamic) {
                auto container = world->GetRenderableContainer(eWorldContainerType_Dynamic);
                container->UpdateBeforeRendering();
                walkShadowCasters(container->GetRoot(), eCollision_Outside);
            }

            if (light->GetShadowCastersAffected() & eObjectVariabilityFlag_Static) {
                auto container = world->GetRenderableContainer(eWorldContainerType_Static);
                container->UpdateBeforeRendering();
                walkShadowCasters(container->GetRoot(), eCollision_Outside);
            }

            // See if any objects where added.
            if (shadowCasters.empty())
                return false;

            // Sort the list
            std::sort(shadowCasters.begin(), shadowCasters.end(), [](iRenderable* a, iRenderable* b) {
                    cMaterial* pMatA = a->GetMaterial();
                    cMaterial* pMatB = b->GetMaterial();

                    //////////////////////////
                    // Alpha mode
                    if (pMatA->GetAlphaMode() != pMatB->GetAlphaMode())
                    {
                        return pMatA->GetAlphaMode() < pMatB->GetAlphaMode();
                    }

                    //////////////////////////
                    // If alpha, sort by texture (we know alpha is same for both materials, so can just test one)
                    if (pMatA->GetAlphaMode() == eMaterialAlphaMode_Trans)
                    {
                        if (pMatA->GetImage(eMaterialTexture_Diffuse) != pMatB->GetImage(eMaterialTexture_Diffuse))
                        {
                            return pMatA->GetImage(eMaterialTexture_Diffuse) < pMatB->GetImage(eMaterialTexture_Diffuse);
                        }
                    }

                    //////////////////////////
                    // View space depth, no need to test further since Z should almost never be the same for two objects.
                    // View space z is really just BB dist dis squared, so use "<"
                    return a->GetViewSpaceZ() < b->GetViewSpaceZ();
                });

            return true;
        }


        static inline cMatrixf GetLightMtx(const DeferredLight& light) {
            switch (light.m_light->GetLightType()) {
            case eLightType_Point:
                return cMath::MatrixScale(light.m_light->GetRadius() * kLightRadiusMul_Medium);
            case eLightType_Spot: {
                cLightSpot* pLightSpot = static_cast<cLightSpot*>(light.m_light);

                float fFarHeight = pLightSpot->GetTanHalfFOV() * pLightSpot->GetRadius() * 2.0f;
                // Note: Aspect might be wonky if there is no gobo.
                float fFarWidth = fFarHeight * pLightSpot->GetAspect();

                return cMath::MatrixScale(
                    cVector3f(fFarWidth, fFarHeight, light.m_light->GetRadius())); // x and y = "far plane", z = radius
            }
            case eLightType_Box: {
                cLightBox* pLightBox = static_cast<cLightBox*>(light.m_light);
                auto mtx =  cMath::MatrixScale(pLightBox->GetSize());
                mtx.SetTranslation(pLightBox->GetWorldPosition());
                return mtx;
            }
            default:
                break;
            }

            return cMatrixf::Identity;
        }


        // this is a bridge to build a render list from the renderable container
        /**
        * updates the render list with objects inside the view frustum
        */
        static inline void UpdateRenderableList(
            cRenderList* renderList, // in/out 
            cVisibleRCNodeTracker* apVisibleNodeTracker,
            cFrustum* frustum,
            cWorld* world,
            tObjectVariabilityFlag objectTypes,
            std::span<cPlanef> occludingPlanes,
            tRenderableFlag alNeededFlags, std::function<void(iRenderable*)> zPassCallback) {
            renderList->Clear();

            auto renderNodeHandler = [&](iRenderableContainerNode* apNode, tRenderableFlag alNeededFlags) {
                if (apNode->HasObjects() == false)
                    return 0;

                int lRenderedObjects = 0;

                ////////////////////////////////////
                // Iterate sorted objects and render
                for (tRenderableListIt it = apNode->GetObjectList()->begin(); it != apNode->GetObjectList()->end(); ++it) {
                    iRenderable* pObject = *it;

                    //////////////////////
                    // Check if object is visible
                    
                    if (!rendering::detail::IsObjectIsVisible(pObject,alNeededFlags,occludingPlanes)) {
                        continue;
                    }
                    
                    renderList->AddObject(pObject);

                    cMaterial* material = pObject->GetMaterial();
                    if(!material || material->GetType()->IsTranslucent()) {
                        continue;
                    }
                    zPassCallback(pObject);
                    // rendering::detail::RenderZPassObject(pass, context, viewport, renderer, pObject);
                    ++lRenderedObjects;
                }

                return lRenderedObjects;
            };

            struct {
                iRenderableContainer* container;
                tObjectVariabilityFlag flag;
            } containers[] = {
                { world->GetRenderableContainer(eWorldContainerType_Static), eObjectVariabilityFlag_Static },
                { world->GetRenderableContainer(eWorldContainerType_Dynamic), eObjectVariabilityFlag_Dynamic },
            };

            // mpCurrentSettings->mlNumberOfOcclusionQueries = 0;

            // Switch sets, so that the last frames set is not current.
            apVisibleNodeTracker->SwitchAndClearVisibleNodeSet();

            for (auto& it : containers) {
                if (it.flag & objectTypes) {
                    it.container->UpdateBeforeRendering();
                }
            }

            // Temp variable used when pushing visibility
            // gpCurrentVisibleNodeTracker = apVisibleNodeTracker;

            // Add Root nodes to stack
            tRendererSortedNodeSet setNodeStack;

            // Render at least X objects without rendering nodes, to some occluders
            const int minimumObjectsBeforeOcclusionTesting = 0;
            // const int onlyRenderPrevVisibleOcclusionObjectsFrameCount = 0;
            int lMinRenderedObjects = minimumObjectsBeforeOcclusionTesting;
            
            std::function<void(iRenderableContainerNode* apNode)> walkRenderables;
            walkRenderables = [&](iRenderableContainerNode* apNode) {
                if(apNode->GetChildNodes().size() > 0) {
                    for(auto& childNode: apNode->GetChildNodes()) {
                        childNode->UpdateBeforeUse();
                        if (childNode->UsesFlagsAndVisibility() &&
                            (childNode->HasVisibleObjects() == false || (childNode->GetRenderFlags() & alNeededFlags) != alNeededFlags))
                        {
                            continue;
                        }
                        eCollision frustumCollision = frustum->CollideNode(childNode);
                        if (frustumCollision == eCollision_Outside) {
                            continue;
                        }

                        if(!rendering::detail::IsRenderableNodeIsVisible(childNode, occludingPlanes)) {
                            continue;
                        }

                        if (frustum->CheckAABBNearPlaneIntersection(childNode->GetMin(), childNode->GetMax())) {
                            cVector3f vViewSpacePos = cMath::MatrixMul(frustum->GetViewMatrix(), childNode->GetCenter());
                            childNode->SetViewDistance(vViewSpacePos.z);
                            childNode->SetInsideView(true);
                        } else { 
                            // Frustum origin is outside of node. Do intersection test.
                            cVector3f vIntersection;
                            cMath::CheckAABBLineIntersection(
                                childNode->GetMin(),
                                childNode->GetMax(),
                                frustum->GetOrigin(),
                                childNode->GetCenter(),
                                &vIntersection,
                                NULL);
                            cVector3f vViewSpacePos = cMath::MatrixMul(frustum->GetViewMatrix(), vIntersection);
                            childNode->SetViewDistance(vViewSpacePos.z);
                            childNode->SetInsideView(false);
                        }
                        walkRenderables(childNode);
                    }
                } else {
                    renderNodeHandler(apNode, alNeededFlags);
                }
            };

            for (auto& it : containers) {
                if (it.flag & objectTypes) {
                    iRenderableContainerNode* pNode = it.container->GetRoot();
                    pNode->UpdateBeforeUse(); // Make sure node is updated.
                    pNode->SetInsideView(true); // We never want to check root! Assume player is inside.
                    walkRenderables(pNode);
                }
            }
        }

        static inline bool SortDeferredLightBox(const DeferredLight* apLightDataA, const DeferredLight* apLightDataB) {
            iLight* pLightA = apLightDataA->m_light;
            iLight* pLightB = apLightDataB->m_light;

            cLightBox* pBoxLightA = static_cast<cLightBox*>(pLightA);
            cLightBox* pBoxLightB = static_cast<cLightBox*>(pLightB);

            if (pBoxLightA->GetBoxLightPrio() != pBoxLightB->GetBoxLightPrio()) {
                return pBoxLightA->GetBoxLightPrio() < pBoxLightB->GetBoxLightPrio();
            }

            //////////////////////////
            // Pointer
            return pLightA < pLightB;
        }

        struct IlluminationOptions {
            const cMatrixf& projectionFrustum;
            const cMatrixf& viewFrustum;
            cRendererDeferred::GBuffer& buffer;
            const char* name;
        };

        static inline void RenderIlluminationPass(
            GraphicsContext& context,
            iRenderer* renderer,
            std::span<iRenderable*> iter,
            cViewport& viewport,
            IlluminationOptions& arg) {
            if (iter.empty()) {
                return;
            }
            // auto bufferSize = arg.buffer.m_outputImage->GetImageSize();
            // GraphicsContext::ViewConfiguration viewConfig{ arg.buffer.m_outputTarget };
            // viewConfig.m_projection = arg.projectionFrustum;
            // viewConfig.m_view = arg.viewFrustum;
            // viewConfig.m_viewRect = { 0, 0, bufferSize.x, bufferSize.y };

            // auto view = context.StartPass(arg.name, viewConfig);
            // rendering::detail::RenderableMaterialIter(
            //     renderer,
            //     iter,
            //     viewport,
            //     eMaterialRenderMode_Illumination,
            //     [&](iRenderable* obj, GraphicsContext::LayoutStream& layoutInput, GraphicsContext::ShaderProgram& shaderInput) {
            //         shaderInput.m_configuration.m_depthTest = DepthTest::Equal;
            //         shaderInput.m_configuration.m_write = Write::RGBA;

            //         shaderInput.m_configuration.m_rgbBlendFunc =
            //             CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
            //         shaderInput.m_configuration.m_alphaBlendFunc =
            //             CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);

            //         shaderInput.m_modelTransform =
            //             obj->GetModelMatrixPtr() ? obj->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity.GetTranspose();

            //         GraphicsContext::DrawRequest drawRequest{ layoutInput, shaderInput };
            //         context.Submit(view, drawRequest);
            //     });
        }
        struct GBufferPassOptions {
            const cMatrixf& projectionFrustum;
            const cMatrixf& viewFrustum;
            cRendererDeferred::GBuffer& buffer;
            const char* name;
        };

        static inline void RenderGBufferPass(
            GraphicsContext& context,
            iRenderer* renderer,
            std::span<iRenderable*> iter,
            cViewport& viewport,
            GBufferPassOptions& arg) {
            if (iter.empty()) {
                return;
            }
            // auto bufferSize = arg.buffer.m_outputImage->GetImageSize();
            // GraphicsContext::ViewConfiguration viewConfig{ arg.buffer.m_fullTarget };
            // viewConfig.m_projection = arg.projectionFrustum;
            // viewConfig.m_view = arg.viewFrustum;
            // viewConfig.m_viewRect = { 0, 0, bufferSize.x, bufferSize.y };

            // auto view = context.StartPass(arg.name, viewConfig);
            // rendering::detail::RenderableMaterialIter(
            //     renderer,
            //     iter,
            //     viewport,
            //     eMaterialRenderMode_Diffuse,
            //     [&context,
            //      view, &arg](iRenderable* obj, GraphicsContext::LayoutStream& layoutInput, GraphicsContext::ShaderProgram& shaderInput) {
            //         shaderInput.m_configuration.m_depthTest = DepthTest::Equal;
            //         shaderInput.m_configuration.m_write = Write::RGBA;
            //         shaderInput.m_configuration.m_cull = Cull::CounterClockwise;

            //         shaderInput.m_modelTransform =
            //             obj->GetModelMatrixPtr() ? obj->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity.GetTranspose();
            //         shaderInput.m_normalMtx =
            //             cMath::MatrixInverse(cMath::MatrixMul(shaderInput.m_modelTransform, arg.viewFrustum)); // matrix is already transposed

            //         GraphicsContext::DrawRequest drawRequest{ layoutInput, shaderInput };
            //         context.Submit(view, drawRequest);
            //     });
        }

        struct DecalPassOptions {
            const cMatrixf& projectionFrustum;
            const cMatrixf& viewFrustum;
            cRendererDeferred::GBuffer& buffer;
            const char* name;
        };

        static inline void RenderDecalPass(
            GraphicsContext& context,
            iRenderer* renderer,
            std::span<iRenderable*> iter,
            cViewport& viewport,
            DecalPassOptions& args) {

            if (iter.empty()) {
                return;
            }
            // auto bufferSize = args.buffer.m_outputImage->GetImageSize();
            // GraphicsContext::ViewConfiguration viewConfig{ args.buffer.m_colorAndDepthTarget };
            // viewConfig.m_projection = args.projectionFrustum;
            // viewConfig.m_view = args.viewFrustum;
            // viewConfig.m_viewRect = { 0, 0, bufferSize.x, bufferSize.y };

            // auto view = context.StartPass(args.name, viewConfig);
            // rendering::detail::RenderableMaterialIter(
            //     renderer,
            //     iter,
            //     viewport,
            //     eMaterialRenderMode_Diffuse,
            //     [&context, view, &args](iRenderable* obj, GraphicsContext::LayoutStream& layoutInput, GraphicsContext::ShaderProgram& shaderInput) {
            //          shaderInput.m_configuration.m_depthTest = DepthTest::LessEqual;
            //         shaderInput.m_configuration.m_write = Write::RGB;

            //         cMaterial* pMaterial = obj->GetMaterial();
            //         shaderInput.m_configuration.m_rgbBlendFunc = CreateFromMaterialBlendMode(pMaterial->GetBlendMode());
            //         shaderInput.m_configuration.m_alphaBlendFunc = CreateFromMaterialBlendMode(pMaterial->GetBlendMode());
            //         shaderInput.m_modelTransform =
            //             obj->GetModelMatrixPtr() ? obj->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity.GetTranspose();

            //         GraphicsContext::DrawRequest drawRequest{ layoutInput, shaderInput };
            //         context.Submit(view, drawRequest);
            //     });
        }

        // static inline bool RenderShadowPass()

        static inline bool SortDeferredLightDefault(const DeferredLight* a, const DeferredLight* b) {
            iLight* pLightA = a->m_light;
            iLight* pLightB = b->m_light;

            //////////////////////////
            // Type
            if (pLightA->GetLightType() != pLightB->GetLightType()) {
                return pLightA->GetLightType() < pLightB->GetLightType();
            }

            //////////////////////////
            // Specular
            int lHasSpecularA = pLightA->GetDiffuseColor().a > 0 ? 1 : 0;
            int lHasSpecularB = pLightB->GetDiffuseColor().a > 0 ? 1 : 0;
            if (lHasSpecularA != lHasSpecularB) {
                return lHasSpecularA < lHasSpecularB;
            }

            ////////////////////////////////
            // Point inside near plane
            if (pLightA->GetLightType() == eLightType_Point) {
                return a->m_insideNearPlane < b->m_insideNearPlane;
            }

            //////////////////////////
            // Gobo
            if (pLightA->GetGoboTexture() != pLightB->GetGoboTexture()) {
                return pLightA->GetGoboTexture() < pLightB->GetGoboTexture();
            }

            //////////////////////////
            // Attenuation
            if (pLightA->GetFalloffMap() != pLightB->GetFalloffMap()) {
                return pLightA->GetFalloffMap() < pLightB->GetFalloffMap();
            }

            //////////////////////////
            // Spot falloff
            if (pLightA->GetLightType() == eLightType_Spot) {
                cLightSpot* pLightSpotA = static_cast<cLightSpot*>(pLightA);
                cLightSpot* pLightSpotB = static_cast<cLightSpot*>(pLightB);

                if (pLightSpotA->GetSpotFalloffMap() != pLightSpotB->GetSpotFalloffMap()) {
                    return pLightSpotA->GetSpotFalloffMap() < pLightSpotB->GetSpotFalloffMap();
                }
            }

            //////////////////////////
            // Pointer
            return pLightA < pLightB;
        }

        static inline float GetFogAreaVisibilityForObject(const cRendererDeferred::FogRendererData& fogData, cFrustum& frustum, iRenderable* apObject) {
            cFogArea* pFogArea = fogData.m_fogArea;

            cVector3f vObjectPos = apObject->GetBoundingVolume()->GetWorldCenter();
            cVector3f vRayDir = vObjectPos - frustum.GetOrigin();
            float fCameraDistance = vRayDir.Length();
            vRayDir = vRayDir / fCameraDistance;

            float fEntryDist, fExitDist;

            auto checkFogAreaIntersection =
                [&fEntryDist,
                 &fExitDist](const cMatrixf& a_mtxInvBoxModelMatrix, const cVector3f& avBoxSpaceRayStart, const cVector3f& avRayDir) {
                    cVector3f vBoxSpaceDir = cMath::MatrixMul3x3(a_mtxInvBoxModelMatrix, avRayDir);

                    bool bFoundIntersection = false;
                    fExitDist = 0;

                    std::array<cVector3f, 6> fBoxPlaneNormals = { {
                        cVector3f(-1, 0, 0), // Left
                        cVector3f(1, 0, 0), // Right

                        cVector3f(0, -1, 0), // Bottom
                        cVector3f(0, 1, 0), // Top

                        cVector3f(0, 0, -1), // Back
                        cVector3f(0, 0, 1), // Front
                    } };

                    ///////////////////////////////////
                    // Iterate the sides of the cube
                    for (auto& planeNormal : fBoxPlaneNormals) {
                        ///////////////////////////////////
                        // Calculate plane intersection
                        float fMul = cMath::Vector3Dot(planeNormal, vBoxSpaceDir);
                        if (fabs(fMul) < 0.0001f) {
                            continue;
                        }
                        float fNegDist = -(cMath::Vector3Dot(planeNormal, avBoxSpaceRayStart) + 0.5f);

                        float fT = fNegDist / fMul;
                        if (fT < 0)
                            continue;
                        cVector3f vAbsNrmIntersect = cMath::Vector3Abs(vBoxSpaceDir * fT + avBoxSpaceRayStart);

                        ///////////////////////////////////
                        // Check if the intersection is inside the cube
                        if (cMath::Vector3LessEqual(vAbsNrmIntersect, cVector3f(0.5001f))) {
                            //////////////////////
                            // First intersection
                            if (bFoundIntersection == false) {
                                fEntryDist = fT;
                                fExitDist = fT;
                                bFoundIntersection = true;
                            }
                            //////////////////////
                            // There has already been a intersection.
                            else {
                                fEntryDist = cMath::Min(fEntryDist, fT);
                                fExitDist = cMath::Max(fExitDist, fT);
                            }
                        }
                    }

                    if (fExitDist < 0)
                        return false;

                    return bFoundIntersection;
                };
            if (checkFogAreaIntersection(fogData.m_mtxInvBoxSpace, fogData.m_boxSpaceFrustumOrigin, vRayDir) == false) {
                return 1.0f;
            }

            if (fogData.m_insideNearFrustum == false && fCameraDistance < fEntryDist) {
                return 1.0f;
            }

            //////////////////////////////
            // Calculate the distance the ray travels in the fog
            float fFogDist;
            if (fogData.m_insideNearFrustum) {
                if (pFogArea->GetShowBacksideWhenInside())
                    fFogDist = cMath::Min(fExitDist, fCameraDistance);
                else
                    fFogDist = fCameraDistance;
            } else {
                if (pFogArea->GetShowBacksideWhenOutside())
                    fFogDist = cMath::Min(fExitDist - fEntryDist, fCameraDistance - fEntryDist);
                else
                    fFogDist = fCameraDistance - fEntryDist;
            }

            //////////////////////////////
            // Calculate the alpha
            if (fFogDist <= 0)
                return 1.0f;

            float fFogStart = pFogArea->GetStart();
            float fFogEnd = pFogArea->GetEnd();
            float fFogAlpha = 1 - pFogArea->GetColor().a;

            if (fFogDist < fFogStart)
                return 1.0f;

            if (fFogDist > fFogEnd)
                return fFogAlpha;

            float fAlpha = (fFogDist - fFogStart) / (fFogEnd - fFogStart);
            if (pFogArea->GetFalloffExp() != 1)
                fAlpha = powf(fAlpha, pFogArea->GetFalloffExp());

            return (1.0f - fAlpha) + fFogAlpha * fAlpha;
        }
        /**
         * Calculates matrices for both rendering shape and the transformation
         * \param a_mtxDestRender This has a scale based on radius and radius mul. The mul is to make sure that the shape covers the while
         * light. \param a_mtxDestTransform A simple view space transform for the light. \param afRadiusMul
         */
        static inline void SetupLightMatrix(
            cMatrixf& a_mtxDestRender, cMatrixf& a_mtxDestTransform, iLight* apLight, cFrustum* apFrustum, float afRadiusMul) {
            ////////////////////////////
            // Point Light
            if (apLight->GetLightType() == eLightType_Point) {
                a_mtxDestRender =
                    cMath::MatrixScale(apLight->GetRadius() * afRadiusMul); // kLightRadiusMul = make sure it encapsulates the light.
                a_mtxDestTransform = cMath::MatrixMul(apFrustum->GetViewMatrix(), apLight->GetWorldMatrix());
                a_mtxDestRender = cMath::MatrixMul(a_mtxDestTransform, a_mtxDestRender);
            }
            ////////////////////////////
            // Spot Light
            else if (apLight->GetLightType() == eLightType_Spot) {
                cLightSpot* pLightSpot = static_cast<cLightSpot*>(apLight);

                float fFarHeight = pLightSpot->GetTanHalfFOV() * pLightSpot->GetRadius() * 2.0f;
                // Note: Aspect might be wonky if there is no gobo.
                float fFarWidth = fFarHeight * pLightSpot->GetAspect();

                a_mtxDestRender =
                    cMath::MatrixScale(cVector3f(fFarWidth, fFarHeight, apLight->GetRadius())); // x and y = "far plane", z = radius
                a_mtxDestTransform = cMath::MatrixMul(apFrustum->GetViewMatrix(), apLight->GetWorldMatrix());
                a_mtxDestRender = cMath::MatrixMul(a_mtxDestTransform, a_mtxDestRender);
            }
        }
    } // namespace detail

    void cRendererDeferred::InitializeDeferred(ForgeRenderer& pipeline) {
        // static uniform buffer for material data
        detail::MaterialUniformBuffer.TryFree();
        BufferLoadDesc desc = {};
        desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        desc.mDesc.mSize = sizeof(cMaterial::MaterialType::MaterialData) * cMaterial::MaxMaterialID;
        // desc.pData =  &m_info.m_data;
        desc.ppBuffer = &detail::MaterialUniformBuffer.m_handle;
        addResource(&desc, nullptr);
        detail::MaterialUniformBuffer.Initialize();

        // ring buffer for object uniforms
        addUniformGPURingBuffer(pipeline.Rend(), sizeof(cRendererDeferred::CBObjectData) * MaxObjectUniforms, &detail::CObjectRingBuffer, true);
    }



    enum eDefferredProgramMode { eDefferredProgramMode_Lights, eDefferredProgramMode_Misc, eDefferredProgramMode_LastEnum };

    cRendererDeferred::cRendererDeferred(cGraphics* apGraphics, cResources* apResources)
        : iRenderer("Deferred", apGraphics, apResources, eDefferredProgramMode_LastEnum) {
        ////////////////////////////////////
        // Set up render specific things
        mbSetFrameBufferAtBeginRendering = false; // Not using the input frame buffer for any rendering. Only doing copy at the end!
        mbClearFrameBufferAtBeginRendering = false;
        mbSetupOcclusionPlaneForFog = true;

        mfMinLargeLightNormalizedArea = 0.2f * 0.2f;

        m_shadowDistanceMedium = 10;
        m_shadowDistanceLow = 20;
        m_shadowDistanceNone = 40;

        m_maxBatchLights = 100;

        m_boundViewportData = std::move(UniqueViewportData<SharedViewportData>(
            [](cViewport& viewport) {
                auto sharedData = std::make_unique<SharedViewportData>();
                sharedData->m_size = viewport.GetSize();
                auto* forgetRenderer = Interface<ForgeRenderer>::Get();
                ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
                for(auto& b : sharedData->m_gBuffer) {
                   auto deferredRenderTargetDesc = [&]() {
                        RenderTargetDesc renderTarget = {};
                        renderTarget.mArraySize = 1;
                        renderTarget.mClearValue = optimizedColorClearBlack;
                        renderTarget.mDepth = 1;
                        renderTarget.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
                        renderTarget.mWidth = sharedData->m_size.x;
                        renderTarget.mHeight = sharedData->m_size.y;
                        renderTarget.mSampleCount = SAMPLE_COUNT_1;
                        renderTarget.mSampleQuality = 0;
                        renderTarget.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
                        renderTarget.pName = "G-Buffer RTs";
                        return renderTarget;
                   };
                   
                   {
                        auto depthRT = deferredRenderTargetDesc();
                        depthRT.mFormat = DepthBufferFormat;
                        depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
                        ForgeRenderTarget target = {forgetRenderer->Rend()};
                        addRenderTarget(forgetRenderer->Rend(), &depthRT, &target.m_handle);
                        target.Initialize();
                        b.m_depthBuffer = std::move(target);

                   }
                   {
                        auto normalRT = deferredRenderTargetDesc();
                        normalRT.mFormat = NormalBufferFormat;
                        ForgeRenderTarget target = {forgetRenderer->Rend()};
                        addRenderTarget(forgetRenderer->Rend(), &normalRT, &target.m_handle);
                        target.Initialize();
                        b.m_normalBuffer = std::move(target);
                   }
                   {
                        auto positionRT = deferredRenderTargetDesc();
                        positionRT.mFormat = PositionBufferFormat;
                        ForgeRenderTarget target = {forgetRenderer->Rend()};
                        addRenderTarget(forgetRenderer->Rend(), &positionRT, &target.m_handle);
                        target.Initialize();
                        b.m_positionBuffer = std::move(target);
                   }
                   {
                        auto specularRT = deferredRenderTargetDesc();
                        specularRT.mFormat = SpecularBufferFormat;
                        ForgeRenderTarget target = {forgetRenderer->Rend()};
                        addRenderTarget(forgetRenderer->Rend(), &specularRT, &target.m_handle);
                        target.Initialize();
                        b.m_specularBuffer = std::move(target);
                   }
                   {
                        auto colorRT = deferredRenderTargetDesc();
                        colorRT.mFormat = ColorBufferFormat;
                        ForgeRenderTarget target = {forgetRenderer->Rend()};
                        addRenderTarget(forgetRenderer->Rend(), &colorRT, &target.m_handle);
                        target.Initialize();
                        b.m_colorBuffer = std::move(target);
                   }
                   {

                        auto outputRt = deferredRenderTargetDesc();
                        outputRt.mFormat = getRecommendedSwapchainFormat(false, false);
                        outputRt.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
                        
                        ForgeRenderTarget target = {forgetRenderer->Rend()};
                        addRenderTarget(forgetRenderer->Rend(), &outputRt, &target.m_handle);
                        target.Initialize();
                        b.m_outputBuffer = std::move(target);
                   }
                }

                // sharedData->m_gBuffer = buildGBuffer();
                // sharedData->m_gBufferReflection = buildGBuffer();

                // {
                //     auto desc = ImageDescriptor::CreateTexture2D(
                //         sharedData->m_size.x, sharedData->m_size.y, false, bgfx::TextureFormat::Enum::RGBA8);
                //     desc.m_configuration.m_computeWrite = true;
                //     desc.m_configuration.m_rt = RTType::RT_Write;
                //     auto image = std::make_shared<Image>();
                //     image->Initialize(desc);
                //     sharedData->m_refractionImage = image;
                // }

                return sharedData;
            },
            [](cViewport& viewport, SharedViewportData& target) {
                return target.m_size == viewport.GetSize();
            }));

        ////////////////////////////////////
        // Create Shadow Textures
        cVector3l vShadowSize[] = { cVector3l(2 * 128, 2 * 128, 1),
                                    cVector3l(2 * 256, 2 * 256, 1),
                                    cVector3l(2 * 256, 2 * 256, 1),
                                    cVector3l(2 * 512, 2 * 512, 1),
                                    cVector3l(2 * 1024, 2 * 1024, 1) };
        int lStartSize = 2;
        if (mShadowMapResolution == eShadowMapResolution_Medium) {
            lStartSize = 1;
        } else if (mShadowMapResolution == eShadowMapResolution_Low) {
            lStartSize = 0;
        }

        m_dissolveImage = mpResources->GetTextureManager()->Create2DImage("core_dissolve.tga", false);
        
		// for(auto& handle: m_bufferHandle) {

			// m_stageDirtyBits = std::numeric_limits<uint32_t>::max();
		// }

        // per frame constants

        auto* forgetRenderer = Interface<ForgeRenderer>::Get();
        auto updatePerFrameDescriptor = [&](DescriptorSet* desc) {
            for(size_t i = 0; i < ForgeRenderer::SwapChainLength; i++) {
                DescriptorData params[15] = {};
                size_t paramCount = 0;
        
                DescriptorDataRange range = { (uint32_t)(i * sizeof(cRendererDeferred::PerFrameData)) , sizeof(cRendererDeferred::PerFrameData) };
                params[paramCount].pName = "perFrameConstants";
                params[paramCount].pRanges = &range;
                params[paramCount++].ppBuffers = &m_perFrameBuffer.m_handle;

                updateDescriptorSet(forgetRenderer->Rend(),i, desc, paramCount, params);
            }
        };

        auto createMaterialsPass = [&](RootSignature* rootSignature, MaterialPassDescriptorSet& descriptor) {
            DescriptorSetDesc constantDescSet{rootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1};
            addDescriptorSet(forgetRenderer->Rend(), &constantDescSet, &descriptor.m_constSet);
            DescriptorSetDesc perFrameDescSet{rootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, ForgeRenderer::SwapChainLength};
            addDescriptorSet(forgetRenderer->Rend(), &perFrameDescSet, &descriptor.m_frameSet);
            DescriptorSetDesc batchDescriptorSet{rootSignature, DESCRIPTOR_UPDATE_FREQ_PER_BATCH, cMaterial::MaxMaterialID};
            addDescriptorSet(forgetRenderer->Rend(), &batchDescriptorSet, &descriptor.m_materialSet);
            DescriptorSetDesc perObjectDescriptorSet{rootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, MaxObjectUniforms};
            addDescriptorSet(forgetRenderer->Rend(), &perObjectDescriptorSet, &descriptor.m_perObjectSet);
        };

        {
            m_perFrameBuffer.TryFree();
			BufferLoadDesc desc = {};
			desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
			desc.mDesc.mSize = sizeof(cRendererDeferred::PerFrameData) * ForgeRenderer::SwapChainLength; // * cViewport::MaxViewportHandles;
			desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
			desc.pData = nullptr;
			desc.ppBuffer = &m_perFrameBuffer.m_handle;
			addResource(&desc, nullptr);
			m_perFrameBuffer.Initialize();
        }

        
        //---------------- ZPass Pipeline  ------------------------
        {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0] = {"solid_z.vert", nullptr, 0};
            loadDesc.mStages[1] = {"solid_z.frag", nullptr, 0};
	        addShader(forgetRenderer->Rend(), &loadDesc, &m_zPassShader);

	        Shader* zPassShaders[] = {m_zPassShader};
			RootSignatureDesc rootSignatureDesc = {};
			rootSignatureDesc.ppShaders = zPassShaders;
			rootSignatureDesc.mShaderCount = std::size(zPassShaders);
			addRootSignature(forgetRenderer->Rend(), &rootSignatureDesc, &m_zPassRootSignature);

            //layout and pipeline for sphere draw
            VertexLayout vertexLayout = {};
            vertexLayout.mAttribCount = 2;
            vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
            vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
            vertexLayout.mAttribs[0].mBinding = 0;
            vertexLayout.mAttribs[0].mLocation = 0;
            vertexLayout.mAttribs[0].mOffset = 0;

            vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
            vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
            vertexLayout.mAttribs[1].mBinding = 1;
            vertexLayout.mAttribs[1].mLocation = 1;
            vertexLayout.mAttribs[1].mOffset = 0;

            RasterizerStateDesc rasterizerStateDesc = {};
            rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

            DepthStateDesc depthStateDesc = {};
            depthStateDesc.mDepthTest = true;
            depthStateDesc.mDepthWrite = true;
            depthStateDesc.mDepthFunc = CMP_LEQUAL;

            PipelineDesc pipelineDesc = {};
            pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
            auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            pipelineSettings.mRenderTargetCount = 0;
            pipelineSettings.pDepthState = &depthStateDesc;
            pipelineSettings.pColorFormats = NULL;
            pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		    pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
            pipelineSettings.mSampleQuality = 0;
            pipelineSettings.pRootSignature = m_zPassRootSignature;
            pipelineSettings.pShaderProgram = m_zPassShader;
            pipelineSettings.pRasterizerState = &rasterizerStateDesc;
            pipelineSettings.pVertexLayout = &vertexLayout;
            addPipeline(forgetRenderer->Rend(), &pipelineDesc, &m_zPassPipeline);

            createMaterialsPass(m_zPassRootSignature, m_zDescriptorSet);

            std::array<DescriptorData, 1> params{};
            params[0].ppTextures = &m_dissolveImage->GetTexture().m_handle;
            params[0].pName = "dissolveMap";
            updateDescriptorSet(forgetRenderer->Rend(), 0, m_zDescriptorSet.m_constSet, params.size(), params.data());
            // updatePerFrameDescriptor(m_zDescriptorSet.m_frameSet);
        }


        //---------------- Diffuse Pipeline  ------------------------
        {
            {
                ShaderLoadDesc loadDesc = {};
                loadDesc.mStages[0] = {"solid_diffuse.vert", nullptr, 0};
                loadDesc.mStages[1] = {"solid_diffuse.frag", nullptr, 0};
                addShader(forgetRenderer->Rend(), &loadDesc, &m_solidDiffuseShader);
            }

            {
                ShaderLoadDesc loadDesc = {};
                loadDesc.mStages[0] = {"solid_diffuse.vert", nullptr, 0};
                loadDesc.mStages[1] = {"solid_diffuse_parallax.frag", nullptr, 0};
                addShader(forgetRenderer->Rend(), &loadDesc, &m_solidDiffuseParallaxShader);
            }

            Shader* shaders[] = {m_solidDiffuseShader, m_solidDiffuseParallaxShader};
            RootSignatureDesc rootSignatureDesc = {};
			rootSignatureDesc.ppShaders = shaders;
			rootSignatureDesc.mShaderCount = std::size(shaders);
			addRootSignature(forgetRenderer->Rend(), &rootSignatureDesc, &m_solidDiffuseRootSignature);

             //layout and pipeline for sphere draw
            VertexLayout vertexLayout = {};
            vertexLayout.mAttribCount = 4;
            vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
            vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
            vertexLayout.mAttribs[0].mBinding = 0;
            vertexLayout.mAttribs[0].mLocation = 0;
            vertexLayout.mAttribs[0].mOffset = 0;

            vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
            vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
            vertexLayout.mAttribs[1].mBinding = 1;
            vertexLayout.mAttribs[1].mLocation = 1;
            vertexLayout.mAttribs[1].mOffset = 0;

            vertexLayout.mAttribs[2].mSemantic = SEMANTIC_NORMAL;
            vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
            vertexLayout.mAttribs[2].mBinding = 2;
            vertexLayout.mAttribs[2].mLocation = 2;
            vertexLayout.mAttribs[2].mOffset = 0;

            vertexLayout.mAttribs[3].mSemantic = SEMANTIC_TANGENT;
            vertexLayout.mAttribs[3].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
            vertexLayout.mAttribs[3].mBinding = 3;
            vertexLayout.mAttribs[3].mLocation = 3;
            vertexLayout.mAttribs[3].mOffset = 0;

            RasterizerStateDesc rasterizerStateDesc = {};
            rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

            DepthStateDesc depthStateDesc = {};
            depthStateDesc.mDepthTest = true;
            depthStateDesc.mDepthWrite = false;
            depthStateDesc.mDepthFunc = CMP_EQUAL;

            std::array colorFormats = {
                ColorBufferFormat,
                NormalBufferFormat,
                PositionBufferFormat,
                SpecularBufferFormat
            };

            PipelineDesc pipelineDesc = {};
            pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
            auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            pipelineSettings.mRenderTargetCount = colorFormats.size();
            pipelineSettings.pColorFormats = colorFormats.data();
            pipelineSettings.pDepthState = &depthStateDesc;
            pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		    pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
            pipelineSettings.mSampleQuality = 0;
            pipelineSettings.pRootSignature = m_solidDiffuseRootSignature;
            pipelineSettings.pShaderProgram = m_solidDiffuseShader;
            pipelineSettings.pRasterizerState = &rasterizerStateDesc;
            pipelineSettings.pVertexLayout = &vertexLayout;
            addPipeline(forgetRenderer->Rend(), &pipelineDesc, &m_solidDiffusePipeline);

            pipelineSettings.pShaderProgram = m_solidDiffuseParallaxShader;
            addPipeline(forgetRenderer->Rend(), &pipelineDesc, &m_solidDiffuseParallaxPipeline);

            createMaterialsPass(m_solidDiffuseRootSignature, m_solidDescriptorSet);
            updatePerFrameDescriptor(m_solidDescriptorSet.m_frameSet);
        }

        // ------------------------ Light Pass -----------------------------------------------------------------
        {
            addUniformGPURingBuffer(forgetRenderer->Rend(), sizeof(LightUniformData) * MaxLightUniforms, &m_lightPassRingBuffer, true);

            {
                ShaderLoadDesc loadDesc = {};
                loadDesc.mStages[0] = {"deferred_light.vert", nullptr, 0};
                loadDesc.mStages[1] = {"deferred_light_pointlight.frag", nullptr, 0};
                addShader(forgetRenderer->Rend(), &loadDesc, &m_pointLightShader);
            }

            {
                ShaderLoadDesc loadDesc = {};
                loadDesc.mStages[0] = {"deferred_light.vert", nullptr, 0};
                loadDesc.mStages[1] = {"deferred_light_stencil.frag", nullptr, 0};
                addShader(forgetRenderer->Rend(), &loadDesc, &m_stencilLightShader);
            }

            {
                ShaderLoadDesc loadDesc = {};
                loadDesc.mStages[0] = {"deferred_light.vert", nullptr, 0};
                loadDesc.mStages[1] = {"deferred_light_box.frag", nullptr, 0};
                addShader(forgetRenderer->Rend(), &loadDesc, &m_boxLightShader);
            }
            Shader* shaders[] = {m_pointLightShader, m_stencilLightShader, m_boxLightShader};
            RootSignatureDesc rootSignatureDesc = {};
			rootSignatureDesc.ppShaders = shaders;
			rootSignatureDesc.mShaderCount = std::size(shaders);
			addRootSignature(forgetRenderer->Rend(), &rootSignatureDesc, &m_lightPassRootSignature);

            VertexLayout vertexLayout = {};
            vertexLayout.mAttribCount = 1;
            vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
            vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
            vertexLayout.mAttribs[0].mBinding = 0;
            vertexLayout.mAttribs[0].mLocation = 0;
            vertexLayout.mAttribs[0].mOffset = 0;

            DepthStateDesc depthStateDesc = {};
            depthStateDesc.mDepthTest = true;
            depthStateDesc.mDepthWrite = false;
            depthStateDesc.mStencilTest = true;
            depthStateDesc.mDepthFunc = CMP_GEQUAL;
            depthStateDesc.mStencilFrontFunc = CMP_ALWAYS;
            depthStateDesc.mStencilBackFunc = CMP_ALWAYS;
            depthStateDesc.mStencilFrontPass = STENCIL_OP_REPLACE;
            depthStateDesc.mStencilBackPass = STENCIL_OP_REPLACE;
            depthStateDesc.mStencilFrontFail = STENCIL_OP_REPLACE;
            depthStateDesc.mStencilBackFail = STENCIL_OP_REPLACE;
            depthStateDesc.mDepthFrontFail = STENCIL_OP_REPLACE;
            depthStateDesc.mDepthBackFail = STENCIL_OP_REPLACE;
            depthStateDesc.mStencilWriteMask = 0xff;
            depthStateDesc.mStencilReadMask = 0xff;

            DepthStateDesc depthStencilDesc = {};
            depthStencilDesc.mDepthTest = false;
            depthStencilDesc.mDepthWrite = false;
            depthStencilDesc.mStencilTest = true;
            depthStencilDesc.mDepthFunc = CMP_GEQUAL;
            depthStencilDesc.mStencilFrontFunc = CMP_ALWAYS;
            depthStencilDesc.mStencilBackFunc = CMP_ALWAYS;
            depthStencilDesc.mStencilFrontPass = STENCIL_OP_KEEP;
            depthStencilDesc.mStencilBackPass = STENCIL_OP_KEEP;
            depthStencilDesc.mStencilFrontFail = STENCIL_OP_KEEP;
            depthStencilDesc.mStencilBackFail = STENCIL_OP_KEEP;
            depthStencilDesc.mDepthFrontFail = STENCIL_OP_KEEP;
            depthStencilDesc.mDepthBackFail = STENCIL_OP_REPLACE;
            depthStencilDesc.mStencilWriteMask = 0xff;
            depthStencilDesc.mStencilReadMask = 0xff;

            BlendStateDesc blendStateDesc{};
            blendStateDesc.mSrcFactors[0] = BC_ONE;
            blendStateDesc.mDstFactors[0] = BC_ONE;
            blendStateDesc.mBlendModes[0] = BM_ADD;
            blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
            blendStateDesc.mDstAlphaFactors[0] = BC_ONE;
            blendStateDesc.mBlendAlphaModes[0] = BM_ADD;

            std::array colorFormats = {
                getRecommendedSwapchainFormat(false, false)
            };
            {
                PipelineDesc pipelineDesc = {};
                pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
                pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                pipelineSettings.mRenderTargetCount = colorFormats.size();
                pipelineSettings.pColorFormats = colorFormats.data();
                pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
                pipelineSettings.mSampleQuality = 0;
                pipelineSettings.pBlendState = &blendStateDesc;
                pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
                pipelineSettings.pRootSignature = m_lightPassRootSignature;
                pipelineSettings.pVertexLayout = &vertexLayout;

                RasterizerStateDesc rasterizerStateDesc = {};
                rasterizerStateDesc.mCullMode = CULL_MODE_BACK;

                pipelineSettings.pDepthState = &depthStateDesc;
                pipelineSettings.pRasterizerState = &rasterizerStateDesc;
                pipelineSettings.pShaderProgram = m_pointLightShader;
                addPipeline(forgetRenderer->Rend(), &pipelineDesc, &m_pointLightPipeline);
            }

            {

                RasterizerStateDesc rasterizerStateDesc = {};
                rasterizerStateDesc.mCullMode = CULL_MODE_BACK;
                rasterizerStateDesc.mFillMode = FILL_MODE_SOLID;

                PipelineDesc pipelineDesc = {};
                pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
                pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                pipelineSettings.mRenderTargetCount = colorFormats.size();
                pipelineSettings.pColorFormats = colorFormats.data();
                pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
                pipelineSettings.mSampleQuality = 0;
                pipelineSettings.pBlendState = &blendStateDesc;
                pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
                pipelineSettings.pRootSignature = m_lightPassRootSignature;
                pipelineSettings.pVertexLayout = &vertexLayout;

                pipelineSettings.pDepthState = &depthStencilDesc;
                pipelineSettings.pRasterizerState = &rasterizerStateDesc;
                pipelineSettings.pShaderProgram = m_boxLightShader;
                addPipeline(forgetRenderer->Rend(), &pipelineDesc, &m_boxLightPipeline);
            }

            {

                RasterizerStateDesc rasterizerStateDesc{};
                rasterizerStateDesc.mCullMode = CULL_MODE_FRONT;
                rasterizerStateDesc.mFillMode = FILL_MODE_SOLID;

                PipelineDesc pipelineDesc{};
                pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
                auto& pipelineSettings = pipelineDesc.mGraphicsDesc;
                pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
                pipelineSettings.mRenderTargetCount = colorFormats.size();
                pipelineSettings.pColorFormats = colorFormats.data();
                pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
                pipelineSettings.mSampleQuality = 0;
                pipelineSettings.pBlendState = &blendStateDesc;
                pipelineSettings.mDepthStencilFormat = DepthBufferFormat;
                pipelineSettings.pRootSignature = m_lightPassRootSignature;
                pipelineSettings.pVertexLayout = &vertexLayout;
                pipelineSettings.pDepthState = &depthStencilDesc;
                pipelineSettings.pRasterizerState = &rasterizerStateDesc;
                pipelineSettings.pShaderProgram = m_stencilLightShader;
                addPipeline(forgetRenderer->Rend(), &pipelineDesc, &m_lightStencilPipeline);
            }

            DescriptorSetDesc perFrameDescSet{m_lightPassRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, ForgeRenderer::SwapChainLength};
            addDescriptorSet(forgetRenderer->Rend(), &perFrameDescSet, &m_lightFrameSet);
            DescriptorSetDesc batchDescriptorSet{m_lightPassRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_BATCH, cRendererDeferred::MaxLightUniforms};
            addDescriptorSet(forgetRenderer->Rend(), &batchDescriptorSet, &m_lightPerLightSet);
        }

        // auto createShadowMap = [](const cVector3l& avSize) -> ShadowMapData {
        //     auto desc = ImageDescriptor::CreateTexture2D(avSize.x, avSize.y, false, bgfx::TextureFormat::D16F);
        //     desc.m_configuration.m_rt = RTType::RT_Write;
        //     desc.m_configuration.m_minFilter = FilterType::Point;
        //     desc.m_configuration.m_magFilter = FilterType::Point;
        //     desc.m_configuration.m_mipFilter = FilterType::Point;
        //     desc.m_configuration.m_comparsion = DepthTest::LessEqual;
        //     auto image = std::make_shared<Image>();
        //     image->Initialize(desc);
        //     return { 
        //         .m_target = LegacyRenderTarget(image), 
        //         .m_light = nullptr,
        //         .m_transformCount = -1,
        //         .m_frameCount = -1,
        //         .m_radius = 0,
        //         .m_fov = 0,
        //         .m_aspect = 0
        //     };
        // };

        // for (size_t i = 0; i < 10; ++i) {
        //     m_shadowMapData[eShadowMapResolution_High].emplace_back(createShadowMap(vShadowSize[lStartSize + eShadowMapResolution_High]));
        // }
        // for (size_t i = 0; i < 15; ++i) {
        //     m_shadowMapData[eShadowMapResolution_Medium].emplace_back(createShadowMap(vShadowSize[lStartSize + eShadowMapResolution_Medium]));
        // }
        // for (size_t i = 0; i < 20; ++i) {
        //     m_shadowMapData[eShadowMapResolution_Low].emplace_back(createShadowMap(vShadowSize[lStartSize + eShadowMapResolution_Low]));
        // }

        // High
        // int shadowMapJitterSize = 0;
        // int shadowMapJitterSamples = 0;
        // if (mShadowMapQuality == eShadowMapQuality_High) {
        //     shadowMapJitterSize = 64;
        //     shadowMapJitterSamples = 32; // 64 here instead? I mean, ATI has to deal with medium has max? or different max for ATI?
        //     m_spotlightVariants.Initialize(
        //         ShaderHelper::LoadProgramHandlerDefault("vs_deferred_light", "fs_deferred_spotlight_high", false, true));
        // }
        // // Medium
        // else if (mShadowMapQuality == eShadowMapQuality_Medium) {
        //     shadowMapJitterSize = 32;
        //     shadowMapJitterSamples = 16;
        //     m_spotlightVariants.Initialize(
        //         ShaderHelper::LoadProgramHandlerDefault("vs_deferred_light", "fs_deferred_spotlight_medium", false, true));
        // }
        // // Low
        // else {
        //     shadowMapJitterSize = 0;
        //     shadowMapJitterSamples = 0;
        //     m_spotlightVariants.Initialize(
        //         ShaderHelper::LoadProgramHandlerDefault("vs_deferred_light", "fs_deferred_spotlight_low", false, true));
        // }

        // if (mShadowMapQuality != eShadowMapQuality_Low) {
        //     m_shadowJitterImage = std::make_shared<Image>();
        //     TextureCreator::GenerateScatterDiskMap2D(*m_shadowJitterImage, shadowMapJitterSize, shadowMapJitterSamples, true);
        // }
        
        // {
        //     m_ssaoScatterDiskImage = std::make_shared<Image>();
        //     TextureCreator::GenerateScatterDiskMap2D(*m_ssaoScatterDiskImage, 4, SSAONumOfSamples, false);
        // }
        // m_u_param.Initialize();
        // m_u_lightPos.Initialize();
        // m_u_fogColor.Initialize();
        // m_u_lightColor.Initialize();
        // m_u_overrideColor.Initialize();
        // m_u_copyRegion.Initialize();
        // m_u_spotViewProj.Initialize();
        // m_u_mtxInvRotation.Initialize();
        // m_u_mtxInvViewRotation.Initialize();

        // m_s_depthMap.Initialize();
        // m_s_positionMap.Initialize();
        // m_s_diffuseMap.Initialize();
        // m_s_normalMap.Initialize();
        // m_s_specularMap.Initialize();
        // m_s_attenuationLightMap.Initialize();
        // m_s_spotFalloffMap.Initialize();
        // m_s_shadowMap.Initialize();
        // m_s_goboMap.Initialize();
        // m_s_shadowOffsetMap.Initialize();

        // m_s_scatterDisk.Initialize();

        // m_deferredSSAOProgram  = hpl::loadProgram("vs_post_effect", "fs_deferred_ssao");
        // m_deferredSSAOBlurHorizontalProgram  = hpl::loadProgram("vs_post_effect", "fs_deferred_ssao_blur_horizontal");
        // m_deferredSSAOBlurVerticalProgram  = hpl::loadProgram("vs_post_effect", "fs_deferred_ssao_blur_vertical");

        // m_copyRegionProgram = hpl::loadProgram("cs_copy_region");
        // m_lightBoxProgram = hpl::loadProgram("vs_light_box", "fs_light_box");
        // m_nullShader = hpl::loadProgram("vs_null", "fs_null");
        // m_fogVariant.Initialize(ShaderHelper::LoadProgramHandlerDefault("vs_deferred_fog", "fs_deferred_fog", true, true));
        // m_pointLightVariants.Initialize(
        //     ShaderHelper::LoadProgramHandlerDefault("vs_deferred_light", "fs_deferred_pointlight", false, true));
        
        
        ////////////////////////////////////
        // Create SSAO programs and textures
        //  if(mbSSAOLoaded && mpLowLevelGraphics->GetCaps(eGraphicCaps_TextureFloat)==0)
        //  {
        //  	mbSSAOLoaded = false;
        //  	Warning("System does not support float textures! SSAO is disabled.\n");
        //  }
        //  if(mbSSAOLoaded)
        //  {
        //  	cVector2l vSSAOSize = mvScreenSize / mlSSAOBufferSizeDiv;

        // }

        // ////////////////////////////////////
        // //Create Smooth Edge and textures
        // if(mbEdgeSmoothLoaded && mpLowLevelGraphics->GetCaps(eGraphicCaps_TextureFloat)==0)
        // {
        // 	mbEdgeSmoothLoaded = false;
        // 	Warning("System does not support float textures! Edge smooth is disabled.\n");
        // }
        // if(mbEdgeSmoothLoaded)
        // {

        // }

        ////////////////////////////////////
        // Create light shapes
        tFlag lVtxFlag = eVertexElementFlag_Position | eVertexElementFlag_Color0 | eVertexElementFlag_Texture0;
        auto loadVertexBufferFromMesh = [&](const tString& meshName, tVertexElementFlag vtxFlag) {
            iVertexBuffer* pVtxBuffer = mpResources->GetMeshManager()->CreateVertexBufferFromMesh(meshName, vtxFlag);
            if (pVtxBuffer == NULL) {
                FatalError("Could not load vertex buffer from mesh '%s'\n", meshName.c_str());
            }
            return pVtxBuffer;
        };
        m_shapeSphere[eDeferredShapeQuality_High] =
            std::unique_ptr<iVertexBuffer>(loadVertexBufferFromMesh("core_12_12_sphere.dae", lVtxFlag));
        m_shapeSphere[eDeferredShapeQuality_Medium] =
            std::unique_ptr<iVertexBuffer>(loadVertexBufferFromMesh("core_7_7_sphere.dae", lVtxFlag));
        m_shapeSphere[eDeferredShapeQuality_Low] =
            std::unique_ptr<iVertexBuffer>(loadVertexBufferFromMesh("core_5_5_sphere.dae", lVtxFlag));

        m_shapePyramid = std::unique_ptr<iVertexBuffer>(loadVertexBufferFromMesh("core_pyramid.dae", lVtxFlag));

        ////////////////////////////////////
        // Batch vertex buffer
        mlMaxBatchVertices = m_shapeSphere[eDeferredShapeQuality_Low]->GetVertexNum() * m_maxBatchLights;
        mlMaxBatchIndices = m_shapeSphere[eDeferredShapeQuality_Low]->GetIndexNum() * m_maxBatchLights;
    }

    //-----------------------------------------------------------------------
    LegacyRenderTarget& cRendererDeferred::resolveRenderTarget(std::array<LegacyRenderTarget, 2>& rt) {
        return rt[mpCurrentSettings->mbIsReflection ? 1 : 0];
    }

    std::shared_ptr<Image>& cRendererDeferred::resolveRenderImage(std::array<std::shared_ptr<Image>, 2>& img) {
        return img[mpCurrentSettings->mbIsReflection ? 1 : 0];
    }

    cRendererDeferred::~cRendererDeferred() {
    }

    bool cRendererDeferred::LoadData() {
        return true;
    }

    void cRendererDeferred::DestroyData() {
        // for(auto& shape: m_shapeSphere) {
        // 	if(shape) {
        // 		delete shape;
        // 	}
        // }
        // if(m_shapePyramid) hplDelete(m_shapePyramid);

        // mpGraphics->DestroyTexture(mpReflectionTexture);

        /////////////////////////
        // Shadow textures
        //  DestroyShadowMaps();

        // if(mpShadowJitterTexture) mpGraphics->DestroyTexture(mpShadowJitterTexture);

        /////////////////////////
        // Fog stuff
        //  hplDelete(mpFogProgramManager);

        /////////////////////////
        // SSAO textures and programs
        if (mbSSAOLoaded) {
            // mpGraphics->DestroyTexture(mpSSAOTexture);
            // mpGraphics->DestroyTexture(mpSSAOBlurTexture);
            // mpGraphics->DestroyTexture(mpSSAOScatterDisk);

            // mpGraphics->DestroyFrameBuffer(mpSSAOBuffer);
            // mpGraphics->DestroyFrameBuffer(mpSSAOBlurBuffer);

            // mpGraphics->DestroyGpuProgram(mpUnpackDepthProgram);
            // for(int i=0;i<2; ++i)
            // 	mpGraphics->DestroyGpuProgram(mpSSAOBlurProgram[i]);
            // mpGraphics->DestroyGpuProgram(mpSSAORenderProgram);
        }

        /////////////////////////////
        // Edge smooth
        if (mbEdgeSmoothLoaded) {
            // mpGraphics->DestroyFrameBuffer(mpEdgeSmooth_LinearDepthBuffer);

            // mpGraphics->DestroyGpuProgram(mpEdgeSmooth_UnpackDepthProgram);
            // mpGraphics->DestroyGpuProgram(mpEdgeSmooth_RenderProgram);
        }
    }

    // Texture* cRendererDeferred::GetOutputImage(cViewport& viewport) {
    //     // return m_boundViewportData.resolve(viewport).m_gBuffer.m_outputImage;
    //     return nullptr;
    // }
    void cRendererDeferred::RenderFullscreenFogPass(GraphicsContext& context, cWorld* apWorld, cViewport& viewport, cFrustum* apFrustum,  FogPassFullscreenOptions& options) {
        // cVector2l screenSize = viewport.GetSize();
        // GraphicsContext::LayoutStream layout;
        // cMatrixf projMtx;
        // context.ScreenSpaceQuad(layout, projMtx, screenSize.x, screenSize.y);

        // GraphicsContext::ViewConfiguration viewConfig{ options.m_gBuffer.m_outputTarget };
        // viewConfig.m_projection = projMtx;
        // viewConfig.m_viewRect = cRect2l(0, 0, screenSize.x, screenSize.y);
        // const auto view = context.StartPass("Full Screen Fog", viewConfig);

        // struct {
        //     float u_fogStart;
        //     float u_fogLength;
        //     float u_fogFalloffExp;
        // } uniforms = { { 0 } };

        // uniforms.u_fogStart = mpCurrentWorld->GetFogStart();
        // uniforms.u_fogLength = mpCurrentWorld->GetFogEnd() - mpCurrentWorld->GetFogStart();
        // uniforms.u_fogFalloffExp = mpCurrentWorld->GetFogFalloffExp();

        // GraphicsContext::ShaderProgram shaderProgram;
        // shaderProgram.m_configuration.m_write = Write::RGBA;
        // shaderProgram.m_configuration.m_depthTest = DepthTest::Always;

        // shaderProgram.m_configuration.m_rgbBlendFunc =
        //     CreateBlendFunction(BlendOperator::Add, BlendOperand::SrcAlpha, BlendOperand::InvSrcAlpha);
        // shaderProgram.m_configuration.m_write = Write::RGB;

        // cMatrixf rotationMatrix = cMatrixf::Identity;
        // const auto fogColor = mpCurrentWorld->GetFogColor();
        // float uniformFogColor[4] = { fogColor.r, fogColor.g, fogColor.b, fogColor.a };

        // shaderProgram.m_handle = m_fogVariant.GetVariant(rendering::detail::FogVariant_None);

        // shaderProgram.m_textures.push_back({ m_s_positionMap, options.m_gBuffer.m_positionImage->GetHandle(), 0 });

        // shaderProgram.m_uniforms.push_back({ m_u_mtxInvRotation, &rotationMatrix.v });

        // shaderProgram.m_uniforms.push_back({ m_u_param, &uniforms, 1 });
        // shaderProgram.m_uniforms.push_back({ m_u_fogColor, &uniformFogColor });

        // GraphicsContext::DrawRequest drawRequest{ layout, shaderProgram };
        // context.Submit(view, drawRequest);
    }

    void cRendererDeferred::RenderFogPass(GraphicsContext& context, std::span<cRendererDeferred::FogRendererData> fogRenderData, cWorld* apWorld, cViewport& viewport, cFrustum* apFrustum, FogPassOptions& options) {
        // cVector2l screenSize = viewport.GetSize();
        // if(fogRenderData.empty()) {
        //     return;
        // }

        // // ------------------------------------------------------------------------
        // // Render Fog Pass --> output target
        // // ------------------------------------------------------------------------
        // GraphicsContext::ViewConfiguration viewConfig{ options.m_gBuffer.m_outputTarget };
        // viewConfig.m_viewRect = { 0, 0, screenSize.x, screenSize.y };
        // viewConfig.m_view = options.frustumView;
        // viewConfig.m_projection = options.frustumProjection;
        // const auto view = context.StartPass("Fog Pass", viewConfig);
        // for (const auto& fogArea : fogRenderData) {
        //     struct {
        //         float u_fogStart;
        //         float u_fogLength;
        //         float u_fogFalloffExp;

        //         float u_fogRayCastStart[3];
        //         float u_fogNegPlaneDistNeg[3];
        //         float u_fogNegPlaneDistPos[3];
        //     } uniforms;

        //     uniforms.u_fogStart = fogArea.m_fogArea->GetStart();
        //     uniforms.u_fogLength = fogArea.m_fogArea->GetEnd() - fogArea.m_fogArea->GetStart();
        //     uniforms.u_fogFalloffExp = fogArea.m_fogArea->GetFalloffExp();
        //     // Outside of box setup
        //     cMatrixf rotationMatrix = cMatrixf(cMatrixf::Identity);
        //     uint32_t flags = rendering::detail::FogVariant_None;

        //     GraphicsContext::LayoutStream layoutStream;
        //     GraphicsContext::ShaderProgram shaderProgram;

        //     mpShapeBox->GetLayoutStream(layoutStream);

        //     shaderProgram.m_modelTransform =
        //         fogArea.m_fogArea->GetModelMatrixPtr() ? fogArea.m_fogArea->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity;
        //     shaderProgram.m_configuration.m_rgbBlendFunc =
        //         CreateBlendFunction(BlendOperator::Add, BlendOperand::SrcAlpha, BlendOperand::InvSrcAlpha);
        //     shaderProgram.m_configuration.m_alphaBlendFunc =
        //         CreateBlendFunction(BlendOperator::Add, BlendOperand::SrcAlpha, BlendOperand::InvSrcAlpha);
        //     shaderProgram.m_configuration.m_write = Write::RGB;

        //     if (fogArea.m_insideNearFrustum) {
        //         shaderProgram.m_configuration.m_cull = Cull::Clockwise;
        //         shaderProgram.m_configuration.m_depthTest = DepthTest::Always;
        //         flags |= fogArea.m_fogArea->GetShowBacksideWhenInside() ? rendering::detail::FogVariant_UseBackSide
        //                                                                 : rendering::detail::FogVariant_None;
        //     } else {
        //         cMatrixf mtxInvModelView =
        //             cMath::MatrixInverse(cMath::MatrixMul(apFrustum->GetViewMatrix(), *fogArea.m_fogArea->GetModelMatrixPtr()));
        //         cVector3f vRayCastStart = cMath::MatrixMul(mtxInvModelView, cVector3f(0));
        //         // rotationMatrix = mtxInvModelView.GetRotation().GetTranspose();

        //         cVector3f vNegPlaneDistNeg(
        //             cMath::PlaneToPointDist(cPlanef(-1, 0, 0, 0.5f), vRayCastStart),
        //             cMath::PlaneToPointDist(cPlanef(0, -1, 0, 0.5f), vRayCastStart),
        //             cMath::PlaneToPointDist(cPlanef(0, 0, -1, 0.5f), vRayCastStart));
        //         cVector3f vNegPlaneDistPos(
        //             cMath::PlaneToPointDist(cPlanef(1, 0, 0, 0.5f), vRayCastStart),
        //             cMath::PlaneToPointDist(cPlanef(0, 1, 0, 0.5f), vRayCastStart),
        //             cMath::PlaneToPointDist(cPlanef(0, 0, 1, 0.5f), vRayCastStart));

        //         uniforms.u_fogRayCastStart[0] = vRayCastStart.x;
        //         uniforms.u_fogRayCastStart[1] = vRayCastStart.y;
        //         uniforms.u_fogRayCastStart[2] = vRayCastStart.z;

        //         uniforms.u_fogNegPlaneDistNeg[0] = vNegPlaneDistNeg.x * -1;
        //         uniforms.u_fogNegPlaneDistNeg[1] = vNegPlaneDistNeg.y * -1;
        //         uniforms.u_fogNegPlaneDistNeg[2] = vNegPlaneDistNeg.z * -1;

        //         uniforms.u_fogNegPlaneDistPos[0] = vNegPlaneDistPos.x * -1;
        //         uniforms.u_fogNegPlaneDistPos[1] = vNegPlaneDistPos.y * -1;
        //         uniforms.u_fogNegPlaneDistPos[2] = vNegPlaneDistPos.z * -1;

        //         shaderProgram.m_configuration.m_cull = Cull::CounterClockwise;
        //         shaderProgram.m_configuration.m_depthTest = DepthTest::LessEqual;
        //         flags |= rendering::detail::FogVariant_UseOutsideBox;
        //         flags |= fogArea.m_fogArea->GetShowBacksideWhenOutside() ? rendering::detail::FogVariant_UseBackSide
        //                                                                     : rendering::detail::FogVariant_None;
        //     }
        //     const auto fogColor = fogArea.m_fogArea->GetColor();
        //     float uniformFogColor[4] = { fogColor.r, fogColor.g, fogColor.b, fogColor.a };

        //     shaderProgram.m_handle = m_fogVariant.GetVariant(flags);

        //     shaderProgram.m_uniforms.push_back({ m_u_mtxInvRotation, &rotationMatrix.v });

        //     shaderProgram.m_uniforms.push_back({ m_u_param, &uniforms, 3 });
        //     shaderProgram.m_uniforms.push_back({ m_u_fogColor, &uniformFogColor });

        //     shaderProgram.m_textures.push_back({ m_s_positionMap, options.m_gBuffer.m_positionImage->GetHandle(), 0 });

        //     GraphicsContext::DrawRequest drawRequest{ layoutStream, shaderProgram };
        //     context.Submit(view, drawRequest);
        // }
    
    }

    void cRendererDeferred::RenderLightPass(GraphicsContext& context, std::span<iLight*> lights, cWorld* apWorld, cViewport& viewport, cFrustum* apFrustum, LightPassOptions& options) {

        // cVector2l screenSize = viewport.GetSize();

        // float fScreenArea = (float)(screenSize.x * screenSize.y);
        // int mlMinLargeLightArea = (int)(MinLargeLightNormalizedArea * fScreenArea);

        // // std::array<std::vector<detail::DeferredLight*>, eDeferredLightList_LastEnum> sortedLights;
        // // DON'T touch deferredLights after this point
        // // auto lightSpan = lights;
        // std::vector<detail::DeferredLight> deferredLights;
        // deferredLights.reserve(lights.size());
        // std::vector<detail::DeferredLight*> deferredLightBoxRenderBack;
        // std::vector<detail::DeferredLight*> deferredLightBoxStencilFront;

        // std::vector<detail::DeferredLight*> deferredLightRenderBack;
        // std::vector<detail::DeferredLight*> deferredLightStencilFrontRenderBack;

        // for (auto& light : lights) {
        //     auto lightType = light->GetLightType();
        //     auto& deferredLightData = deferredLights.emplace_back(detail::DeferredLight());
        //     deferredLightData.m_light = light;
        //     if (lightType == eLightType_Box) {
        //         continue;
        //     }
        //     switch (lightType) {
        //     case eLightType_Point:
        //         {
        //             deferredLightData.m_insideNearPlane =
        //                 apFrustum->CheckSphereNearPlaneIntersection(light->GetWorldPosition(), light->GetRadius() * kLightRadiusMul_Low);
        //             detail::SetupLightMatrix(
        //                 deferredLightData.m_mtxViewSpaceRender,
        //                 deferredLightData.m_mtxViewSpaceTransform,
        //                 light,
        //                 apFrustum,
        //                 kLightRadiusMul_Medium);
        //             deferredLightData.m_clipRect = cMath::GetClipRectFromSphere(
        //                 deferredLightData.m_mtxViewSpaceRender.GetTranslation(), light->GetRadius(), apFrustum, screenSize, true, 0);
        //             break;
        //         }
        //     case eLightType_Spot:
        //         {
        //             cLightSpot* lightSpot = static_cast<cLightSpot*>(light);
        //             deferredLightData.m_insideNearPlane = apFrustum->CheckFrustumNearPlaneIntersection(lightSpot->GetFrustum());
        //             detail::SetupLightMatrix(
        //                 deferredLightData.m_mtxViewSpaceRender,
        //                 deferredLightData.m_mtxViewSpaceTransform,
        //                 light,
        //                 apFrustum,
        //                 kLightRadiusMul_Medium);
        //             cMath::GetClipRectFromBV(deferredLightData.m_clipRect, *light->GetBoundingVolume(), apFrustum, screenSize, 0);
        //             break;
        //         }
        //     default:
        //         break;
        //     }

        //     if (lightType == eLightType_Spot && light->GetCastShadows() && mpCurrentSettings->mbRenderShadows) {
        //         cLightSpot* pLightSpot = static_cast<cLightSpot*>(light);

        //         ////////////////////////
        //         // Inside near plane, use max resolution
        //         if (deferredLightData.m_insideNearPlane) {
        //             deferredLightData.m_castShadows = true;

        //             deferredLightData.m_shadowResolution = rendering::detail::GetShadowMapResolution(
        //                 light->GetShadowMapResolution(), mpCurrentSettings->mMaxShadowMapResolution);
        //         } else {
        //             cVector3f vIntersection = pLightSpot->GetFrustum()->GetOrigin();
        //             pLightSpot->GetFrustum()->CheckLineIntersection(
        //                 apFrustum->GetOrigin(), light->GetBoundingVolume()->GetWorldCenter(), vIntersection);

        //             float fDistToLight = cMath::Vector3Dist(apFrustum->GetOrigin(), vIntersection);

        //             deferredLightData.m_castShadows = true;
        //             deferredLightData.m_shadowResolution = rendering::detail::GetShadowMapResolution(
        //                 light->GetShadowMapResolution(), mpCurrentSettings->mMaxShadowMapResolution);

        //             ///////////////////////
        //             // Skip shadow
        //             if (fDistToLight > m_shadowDistanceNone) {
        //                 deferredLightData.m_castShadows = false;
        //             }
        //             ///////////////////////
        //             // Use Low
        //             else if (fDistToLight > m_shadowDistanceLow) {
        //                 if (deferredLightData.m_shadowResolution == eShadowMapResolution_Low) {
        //                     deferredLightData.m_castShadows = false;
        //                 }
        //                 deferredLightData.m_shadowResolution = eShadowMapResolution_Low;
        //             }
        //             ///////////////////////
        //             // Use Medium
        //             else if (fDistToLight > m_shadowDistanceMedium) {
        //                 if (deferredLightData.m_shadowResolution == eShadowMapResolution_High) {
        //                     deferredLightData.m_shadowResolution = eShadowMapResolution_Medium;
        //                 } else {
        //                     deferredLightData.m_shadowResolution = eShadowMapResolution_Low;
        //                 }
        //             }
        //         }
        //     }
        // }

        // mpCurrentSettings->mlNumberOfLightsRendered = 0;
        // for (auto& deferredLight : deferredLights) {
        //     // cDeferredLight* pLightData =  mvTempDeferredLights[i];
        //     iLight* pLight = deferredLight.m_light;
        //     eLightType lightType = pLight->GetLightType();

        //     ////////////////////////
        //     // If box, we have special case...
        //     if (lightType == eLightType_Box) {
        //         cLightBox* pLightBox = static_cast<cLightBox*>(pLight);

        //         // Set up matrix
        //         deferredLight.m_mtxViewSpaceRender = cMath::MatrixScale(pLightBox->GetSize());
        //         deferredLight.m_mtxViewSpaceRender.SetTranslation(pLightBox->GetWorldPosition());
        //         deferredLight.m_mtxViewSpaceRender = cMath::MatrixMul(apFrustum->GetViewMatrix(), deferredLight.m_mtxViewSpaceRender);

        //         mpCurrentSettings->mlNumberOfLightsRendered++;

        //         // Check if near plane is inside box. If so only render back
        //         if (apFrustum->CheckBVNearPlaneIntersection(pLight->GetBoundingVolume())) {
        //             deferredLightBoxRenderBack.emplace_back(&deferredLight);
        //         } else {
        //             deferredLightBoxStencilFront.emplace_back(&deferredLight);
        //         }

        //         continue;
        //     }

        //     mpCurrentSettings->mlNumberOfLightsRendered++;

        //     if (deferredLight.m_insideNearPlane) {
        //         deferredLightRenderBack.emplace_back(&deferredLight);
        //     } else {
        //         if (lightType == eLightType_Point) {
        //             if (deferredLight.getArea() >= mlMinLargeLightArea) {
        //                 deferredLightStencilFrontRenderBack.emplace_back(&deferredLight);
        //             } else {
        //                 deferredLightRenderBack.emplace_back(&deferredLight);
        //             }
        //         }
        //         // Always do double passes for spotlights as they need to will get artefacts otherwise...
        //         //(At least with gobos)l
        //         else if (lightType == eLightType_Spot) {
        //             deferredLightStencilFrontRenderBack.emplace_back(&deferredLight);
        //         }
        //     }
        // }
        // std::sort(deferredLightBoxRenderBack.begin(), deferredLightBoxRenderBack.end(), detail::SortDeferredLightBox);
        // std::sort(deferredLightBoxStencilFront.begin(), deferredLightBoxStencilFront.end(), detail::SortDeferredLightBox);
        // std::sort(deferredLightRenderBack.begin(), deferredLightRenderBack.end(), detail::SortDeferredLightDefault);
        // std::sort(deferredLightStencilFrontRenderBack.begin(), deferredLightStencilFrontRenderBack.end(), detail::SortDeferredLightDefault);

        // {
        //     GraphicsContext::ViewConfiguration clearBackBufferView{ options.m_gBuffer.m_outputTarget };
        //     clearBackBufferView.m_clear = { 0, 1, 0, ClearOp::Color };
        //     clearBackBufferView.m_viewRect = {
        //         0, 0, static_cast<uint16_t>(screenSize.x), static_cast<uint16_t>(screenSize.y)
        //     };
        //     bgfx::touch(context.StartPass("Clear Backbuffer", clearBackBufferView));
        // }
        // // -----------------------------------------------
        // // Draw Box Lights
        // // -----------------------------------------------
        // {
        //     auto drawBoxLight = [&](bgfx::ViewId view, GraphicsContext::ShaderProgram& shaderProgram, detail::DeferredLight* light) {
        //         GraphicsContext::LayoutStream layoutStream;
        //         mpShapeBox->GetLayoutStream(layoutStream);
        //         cLightBox* pLightBox = static_cast<cLightBox*>(light->m_light);

        //         const auto& color = light->m_light->GetDiffuseColor();
        //         float lightColor[4] = { color.r, color.g, color.b, color.a };
        //         shaderProgram.m_handle = m_lightBoxProgram;
        //         shaderProgram.m_textures.push_back({ m_s_diffuseMap, options.m_gBuffer.m_colorImage->GetHandle(), 0 });
        //         shaderProgram.m_uniforms.push_back({ m_u_lightColor, lightColor });

        //        shaderProgram.m_modelTransform = detail::GetLightMtx(*light).GetTranspose();

        //         switch (pLightBox->GetBlendFunc()) {
        //         case eLightBoxBlendFunc_Add:
        //             shaderProgram.m_configuration.m_rgbBlendFunc =
        //                 CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
        //             shaderProgram.m_configuration.m_alphaBlendFunc =
        //                 CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
        //             break;
        //         case eLightBoxBlendFunc_Replace:
        //             break;
        //         default:
        //             BX_ASSERT(false, "Unknown blend func %d", pLightBox->GetBlendFunc());
        //             break;
        //         }

        //         GraphicsContext::DrawRequest drawRequest{ layoutStream, shaderProgram };
        //         context.Submit(view, drawRequest);
        //     };

        //     GraphicsContext::ViewConfiguration stencilFrontBackViewConfig{ options.m_gBuffer.m_outputTarget };
        //     stencilFrontBackViewConfig.m_projection = options.frustumProjection;
        //     stencilFrontBackViewConfig.m_view = options.frustumView;
        //     stencilFrontBackViewConfig.m_clear = { 0, 1.0, 0, ClearOp::Stencil };
        //     stencilFrontBackViewConfig.m_viewRect = {
        //         0, 0, static_cast<uint16_t>(screenSize.x), static_cast<uint16_t>(screenSize.y)
        //     };
        //     const auto boxStencilPass = context.StartPass("eDeferredLightList_Box_StencilFront_RenderBack", stencilFrontBackViewConfig);
        //     bgfx::setViewMode(boxStencilPass, bgfx::ViewMode::Sequential);

        //     for (auto& light : deferredLightBoxStencilFront) {
        //         {
        //             GraphicsContext::ShaderProgram shaderProgram;
        //             GraphicsContext::LayoutStream layoutStream;
        //             mpShapeBox->GetLayoutStream(layoutStream);

        //             shaderProgram.m_handle = m_nullShader;
        //             shaderProgram.m_configuration.m_cull = Cull::CounterClockwise;
        //             shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
        //             shaderProgram.m_configuration.m_frontStencilTest = CreateStencilTest(
        //                 StencilFunction::Always, StencilFail::Keep, StencilDepthFail::Replace, StencilDepthPass::Keep, 0xff, 0xff);

        //             shaderProgram.m_modelTransform = detail::GetLightMtx(*light).GetTranspose();

        //             GraphicsContext::DrawRequest drawRequest{ layoutStream, shaderProgram };
        //             // drawRequest.m_clear =  GraphicsContext::ClearRequest{0, 0, 0, ClearOp::Stencil};
        //             context.Submit(boxStencilPass, drawRequest);
        //         }

        //         {
        //             GraphicsContext::ShaderProgram shaderProgram;
        //             shaderProgram.m_configuration.m_cull = Cull::Clockwise;
        //             shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
        //             shaderProgram.m_configuration.m_write = Write::RGB;
        //             shaderProgram.m_configuration.m_frontStencilTest = CreateStencilTest(
        //                 StencilFunction::Equal, StencilFail::Zero, StencilDepthFail::Zero, StencilDepthPass::Zero, 0xff, 0xff);
        //             drawBoxLight(boxStencilPass, shaderProgram, light);
        //         }
        //     }

        //     GraphicsContext::ViewConfiguration boxLightConfig{ options.m_gBuffer.m_outputTarget };
        //     boxLightConfig.m_projection = options.frustumProjection;
        //     boxLightConfig.m_view = options.frustumView;
        //     boxLightConfig.m_viewRect = { 0, 0, static_cast<uint16_t>(screenSize.x), static_cast<uint16_t>(screenSize.y) };
        //     const auto boxLightBackPass = context.StartPass("eDeferredLightList_Box_RenderBack", boxLightConfig);
        //     for (auto& light : deferredLightBoxRenderBack) {
        //         GraphicsContext::ShaderProgram shaderProgram;
        //         shaderProgram.m_configuration.m_cull = Cull::Clockwise;
        //         shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
        //         shaderProgram.m_configuration.m_write = Write::RGB;
        //         drawBoxLight(boxLightBackPass, shaderProgram, light);
        //     }
        // }

        // // -----------------------------------------------
        // // Draw Point Lights
        // // Draw Spot Lights
        // // -----------------------------------------------
        // {
        //     auto drawLight = [&](bgfx::ViewId pass, GraphicsContext::ShaderProgram& shaderProgram, detail::DeferredLight* apLightData) {
        //         GraphicsContext::LayoutStream layoutStream;
        //         GetLightShape(apLightData->m_light, eDeferredShapeQuality_High)->GetLayoutStream(layoutStream);
        //         GraphicsContext::DrawRequest drawRequest{ layoutStream, shaderProgram };
        //         switch (apLightData->m_light->GetLightType()) {
        //         case eLightType_Point:
        //             {
        //                 struct {
        //                     float lightRadius;
        //                     float pad[3];
        //                 } param = { 0 };
        //                 param.lightRadius = apLightData->m_light->GetRadius();
        //                 auto attenuationImage = apLightData->m_light->GetFalloffMap();

        //                 const auto modelViewMtx = cMath::MatrixMul(apFrustum->GetViewMatrix(), apLightData->m_light->GetWorldMatrix());
        //                 const auto color = apLightData->m_light->GetDiffuseColor();
        //                 cVector3f lightViewPos = cMath::MatrixMul(modelViewMtx, detail::GetLightMtx(*apLightData)).GetTranslation();
        //                 float lightPosition[4] = { lightViewPos.x, lightViewPos.y, lightViewPos.z, 1.0f };
        //                 float lightColor[4] = { color.r, color.g, color.b, color.a };
        //                 cMatrixf mtxInvViewRotation =
        //                     cMath::MatrixMul(apLightData->m_light->GetWorldMatrix(), options.frustumInvView).GetTranspose();

        //                 shaderProgram.m_uniforms.push_back({ m_u_lightPos, lightPosition });
        //                 shaderProgram.m_uniforms.push_back({ m_u_lightColor, lightColor });
        //                 shaderProgram.m_uniforms.push_back({ m_u_param, &param });
        //                 shaderProgram.m_uniforms.push_back({ m_u_mtxInvViewRotation, mtxInvViewRotation.v });

        //                 // shaderProgram.m_textures.push_back({ m_s_diffuseMap, options.m_gBuffer.m_colorImage->GetHandle(), 1 });
        //                 // shaderProgram.m_textures.push_back({ m_s_normalMap, options.m_gBuffer.m_normalImage->GetHandle(), 2 });
        //                 // shaderProgram.m_textures.push_back({ m_s_positionMap, options.m_gBuffer.m_positionImage->GetHandle(), 3 });
        //                 // shaderProgram.m_textures.push_back({ m_s_specularMap, options.m_gBuffer.m_specularImage->GetHandle(), 4 });
        //                 shaderProgram.m_textures.push_back({ m_s_attenuationLightMap, attenuationImage->GetHandle(), 5 });

        //                 uint32_t flags = 0;
        //                 if (apLightData->m_light->GetGoboTexture()) {
        //                     flags |= rendering::detail::PointlightVariant_UseGoboMap;
        //                     shaderProgram.m_textures.push_back({ m_s_goboMap, apLightData->m_light->GetGoboTexture()->GetHandle(), 0 });
        //                 }
        //                 shaderProgram.m_handle = m_pointLightVariants.GetVariant(flags);
        //                 context.Submit(pass, drawRequest);
        //                 break;
        //             }
        //         case eLightType_Spot:
        //             {
        //                 cLightSpot* pLightSpot = static_cast<cLightSpot*>(apLightData->m_light);
        //                 // Calculate and set the forward vector
        //                 cVector3f vForward = cVector3f(0, 0, 1);
        //                 vForward = cMath::MatrixMul3x3(apLightData->m_mtxViewSpaceTransform, vForward);

        //                 struct {
        //                     float lightRadius;
        //                     float lightForward[3];

        //                     float oneMinusCosHalfSpotFOV;
        //                     float shadowMapOffset[2];
        //                     float pad;
        //                 } uParam = { apLightData->m_light->GetRadius(),
        //                              { vForward.x, vForward.y, vForward.z },
        //                              1 - pLightSpot->GetCosHalfFOV(),
        //                              { 0, 0 },
        //                              0

        //                 };
        //                 auto goboImage = apLightData->m_light->GetGoboTexture();
        //                 auto spotFallOffImage = pLightSpot->GetSpotFalloffMap();
        //                 auto spotAttenuationImage = pLightSpot->GetFalloffMap();

        //                 uint32_t flags = 0;
        //                 const auto modelViewMtx = cMath::MatrixMul(apFrustum->GetViewMatrix(), apLightData->m_light->GetWorldMatrix());
        //                 const auto color = apLightData->m_light->GetDiffuseColor();
        //                 cVector3f lightViewPos = cMath::MatrixMul(modelViewMtx, detail::GetLightMtx(*apLightData)).GetTranslation();
        //                 float lightPosition[4] = { lightViewPos.x, lightViewPos.y, lightViewPos.z, 1.0f };
        //                 float lightColor[4] = { color.r, color.g, color.b, color.a };
        //                 cMatrixf spotViewProj = cMath::MatrixMul(pLightSpot->GetViewProjMatrix(), options.frustumInvView).GetTranspose();

        //                 shaderProgram.m_uniforms.push_back({ m_u_lightPos, lightPosition });
        //                 shaderProgram.m_uniforms.push_back({ m_u_lightColor, lightColor });
        //                 shaderProgram.m_uniforms.push_back({ m_u_spotViewProj, &spotViewProj.v });

        //                 // shaderProgram.m_textures.push_back({ m_s_diffuseMap, options.m_gBuffer.m_colorImage->GetHandle(), 0 });
        //                 // shaderProgram.m_textures.push_back({ m_s_normalMap, options.m_gBuffer.m_normalImage->GetHandle(), 1 });
        //                 // shaderProgram.m_textures.push_back({ m_s_positionMap, options.m_gBuffer.m_positionImage->GetHandle(), 2 });
        //                 // shaderProgram.m_textures.push_back({ m_s_specularMap, options.m_gBuffer.m_specularImage->GetHandle(), 3 });

        //                 shaderProgram.m_textures.push_back({ m_s_attenuationLightMap, spotAttenuationImage->GetHandle(), 4 });
        //                 if (goboImage) {
        //                     shaderProgram.m_textures.push_back({ m_s_goboMap, goboImage->GetHandle(), 5 });
        //                     flags |= rendering::detail::SpotlightVariant_UseGoboMap;
        //                 } else {
        //                     shaderProgram.m_textures.push_back({ m_s_spotFalloffMap, spotFallOffImage->GetHandle(), 5 });
        //                 }
        //                 auto& currentLight = apLightData->m_light; 
        //                 BX_ASSERT(currentLight->GetLightType() == eLightType_Spot, "Only spot lights are supported for shadow rendering")
                        
        //                 cLightSpot* pSpotLight = static_cast<cLightSpot*>(currentLight);
        //                 cFrustum* pLightFrustum = pSpotLight->GetFrustum();

        //                 std::vector<iRenderable*> shadowCasters;
        //                 if (apLightData->m_castShadows &&
        //                     detail::SetupShadowMapRendering(shadowCasters, apWorld, pLightFrustum, pLightSpot, mvCurrentOcclusionPlanes)) {
        //                     flags |= rendering::detail::SpotlightVariant_UseShadowMap;
        //                     eShadowMapResolution shadowMapRes = apLightData->m_shadowResolution;

        //                     auto findBestShadowMap = [&](eShadowMapResolution resolution,
        //                                                  iLight* light) -> cRendererDeferred::ShadowMapData* {
        //                         auto& shadowMapVec = m_shadowMapData[resolution];
        //                         int maxFrameDistance = -1;
        //                         size_t bestIndex = 0;
        //                         for (size_t i = 0; i < shadowMapVec.size(); ++i) {
        //                             auto& shadowMap = shadowMapVec[i];
        //                             if (shadowMap.m_light == light) {
        //                                 shadowMap.m_frameCount = iRenderer::GetRenderFrameCount();
        //                                 return &shadowMap;
        //                             }

        //                             const int frameDist = cMath::Abs(shadowMap.m_frameCount - iRenderer::GetRenderFrameCount());
        //                             if (frameDist > maxFrameDistance) {
        //                                 maxFrameDistance = frameDist;
        //                                 bestIndex = i;
        //                             }
        //                         }
        //                         if (maxFrameDistance != -1) {
        //                             shadowMapVec[bestIndex].m_frameCount = iRenderer::GetRenderFrameCount();
        //                             return &shadowMapVec[bestIndex];
        //                         }
        //                         return nullptr;
        //                     };
        //                     auto* shadowMapData = findBestShadowMap(shadowMapRes, currentLight);
        //                     if (!shadowMapData) {
        //                         // No shadow map available
        //                         BX_ASSERT(false, "No shadow map available");
        //                         break;
        //                     }
        //                     const auto shadowMapSize = shadowMapData->m_target.GetImage()->GetImageSize();
        //                     // testing if the shadow map needs to be updated
        //                     if ([&]() -> bool {
        //                             // Check if texture map and light are valid
        //                             if (currentLight->GetOcclusionCullShadowCasters()) {
        //                                 return true;
        //                             }

        //                             if (currentLight->GetLightType() == eLightType_Spot &&
        //                                 (pSpotLight->GetAspect() != shadowMapData->m_aspect ||
        //                                  pSpotLight->GetFOV() != shadowMapData->m_fov)) {
        //                                 return true;
        //                             }
        //                             return !currentLight->ShadowCastersAreUnchanged(shadowCasters);
        //                         }()) {
        //                         shadowMapData->m_light = currentLight;
        //                         shadowMapData->m_transformCount = currentLight->GetTransformUpdateCount();
        //                         shadowMapData->m_radius = currentLight->GetRadius();

        //                         if (currentLight->GetLightType() == eLightType_Spot) {
        //                             shadowMapData->m_aspect = pSpotLight->GetAspect();
        //                             shadowMapData->m_fov = pSpotLight->GetFOV();
        //                         }
        //                         currentLight->SetShadowCasterCacheFromVec(shadowCasters);

        //                         GraphicsContext::ViewConfiguration shadowPassViewConfig{ shadowMapData->m_target };
        //                         shadowPassViewConfig.m_clear = { 0, 1.0, 0, ClearOp::Depth };
        //                         shadowPassViewConfig.m_view = pLightFrustum->GetViewMatrix().GetTranspose();
        //                         shadowPassViewConfig.m_projection = pLightFrustum->GetProjectionMatrix().GetTranspose();
        //                         shadowPassViewConfig.m_viewRect = {
        //                             0, 0, static_cast<uint16_t>(shadowMapSize.x), static_cast<uint16_t>(shadowMapSize.y)
        //                         };
        //                         bgfx::ViewId view = context.StartPass("Shadow Pass", shadowPassViewConfig);
        //                         for (auto& shadowCaster : shadowCasters) {
        //                             rendering::detail::RenderZPassObject(
        //                                 view,
        //                                 context,
        //                                 viewport,
        //                                 this,
        //                                 shadowCaster,
        //                                 pLightFrustum->GetInvertsCullMode() ? Cull::Clockwise : Cull::CounterClockwise);
        //                         }
        //                     }
        //                     uParam.shadowMapOffset[0] = 1.0f / shadowMapSize.x;
        //                     uParam.shadowMapOffset[1] = 1.0f / shadowMapSize.y;
        //                     if (m_shadowJitterImage) {
        //                         shaderProgram.m_textures.push_back({ m_s_shadowOffsetMap, m_shadowJitterImage->GetHandle(), 7 });
        //                     }
        //                     shaderProgram.m_textures.push_back({ m_s_shadowMap, shadowMapData->m_target.GetImage()->GetHandle(), 6 });
        //                 }
        //                 shaderProgram.m_uniforms.push_back({ m_u_param, &uParam, 2 });
        //                 shaderProgram.m_handle = m_spotlightVariants.GetVariant(flags);

        //                 context.Submit(pass, drawRequest);
        //                 break;
        //             }
        //         default:
        //             break;
        //         }
        //     };

        //     GraphicsContext::ViewConfiguration viewConfig{ options.m_gBuffer.m_outputTarget };
        //     viewConfig.m_viewRect = { 0, 0, screenSize.x, screenSize.y };
        //     viewConfig.m_projection = options.frustumProjection;
        //     viewConfig.m_view = options.frustumView;
        //     const auto lightStencilBackPass = context.StartPass("eDeferredLightList_StencilFront_RenderBack", viewConfig);
        //     bgfx::setViewMode(lightStencilBackPass, bgfx::ViewMode::Sequential);

        //     for (auto& light : deferredLightStencilFrontRenderBack) {
        //         {
        //             GraphicsContext::ShaderProgram shaderProgram;
        //             GraphicsContext::LayoutStream layoutStream;

        //             GetLightShape(light->m_light, eDeferredShapeQuality_Medium)->GetLayoutStream(layoutStream);

        //             shaderProgram.m_handle = m_nullShader;
        //             shaderProgram.m_configuration.m_cull = Cull::CounterClockwise;
        //             shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;

        //             shaderProgram.m_modelTransform =
        //                 cMath::MatrixMul(light->m_light->GetWorldMatrix(), detail::GetLightMtx(*light)).GetTranspose();

        //             shaderProgram.m_configuration.m_frontStencilTest = CreateStencilTest(
        //                 StencilFunction::Always, StencilFail::Keep, StencilDepthFail::Replace, StencilDepthPass::Keep, 0xff, 0xff);

        //             GraphicsContext::DrawRequest drawRequest{ layoutStream, shaderProgram };
        //             // drawRequest.m_clear =  GraphicsContext::ClearRequest{0, 0, 0, ClearOp::Stencil};
        //             // drawRequest.m_width = mvScreenSize.x;
        //             // drawRequest.m_height = mvScreenSize.y;
        //             // if(bgfx::isValid(light->m_occlusionQuery)) {
        //             // 	bgfx::setCondition(light->m_occlusionQuery, true);
        //             // }
        //             context.Submit(lightStencilBackPass, drawRequest);
        //         }
        //         {
        //             GraphicsContext::ShaderProgram shaderProgram;
        //             shaderProgram.m_configuration.m_cull = Cull::Clockwise;
        //             shaderProgram.m_configuration.m_write = Write::RGB;
        //             shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
        //             shaderProgram.m_configuration.m_frontStencilTest = CreateStencilTest(
        //                 StencilFunction::Equal, StencilFail::Zero, StencilDepthFail::Zero, StencilDepthPass::Zero, 0xff, 0xff);
        //             shaderProgram.m_configuration.m_rgbBlendFunc =
        //                 CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
        //             shaderProgram.m_configuration.m_alphaBlendFunc =
        //                 CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);

        //             shaderProgram.m_modelTransform =
        //                 cMath::MatrixMul(light->m_light->GetWorldMatrix(), detail::GetLightMtx(*light)).GetTranspose();

        //             drawLight(lightStencilBackPass, shaderProgram, light);
        //         }
        //     }

        //     GraphicsContext::ViewConfiguration lightBackPassConfig{ options.m_gBuffer.m_outputTarget };
        //     lightBackPassConfig.m_projection = options.frustumProjection;
        //     lightBackPassConfig.m_view = options.frustumView;
        //     lightBackPassConfig.m_viewRect = { 0, 0, screenSize.x, screenSize.y };
        //     const auto lightBackPass = context.StartPass("eDeferredLightList_RenderBack", lightBackPassConfig);
        //     for (auto& light : deferredLightRenderBack) {
        //         GraphicsContext::ShaderProgram shaderProgram;
        //         shaderProgram.m_configuration.m_cull = Cull::Clockwise;
        //         shaderProgram.m_configuration.m_write = Write::RGB;
        //         shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
        //         shaderProgram.m_configuration.m_rgbBlendFunc =
        //             CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
        //         shaderProgram.m_configuration.m_alphaBlendFunc =
        //             CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);

        //         shaderProgram.m_modelTransform =
        //             cMath::MatrixMul(light->m_light->GetWorldMatrix(), detail::GetLightMtx(*light)).GetTranspose();

        //         drawLight(lightBackPass, shaderProgram, light);
        //     }
        // }
    }

    void cRendererDeferred::RenderEdgeSmoothPass(GraphicsContext& context, cViewport& viewport, LegacyRenderTarget& rt) {
        GraphicsContext::ViewConfiguration viewConfig{ m_edgeSmooth_LinearDepth };
        auto edgeSmoothView = context.StartPass("EdgeSmooth", viewConfig);
        cVector3f vQuadPos = cVector3f(m_farLeft, m_farBottom, -m_farPlane);
        cVector2f vQuadSize = cVector2f(m_farRight * 2, m_farTop * 2);

        GraphicsContext::ShaderProgram shaderProgram;
        // shaderProgram.m_handle = m_edgeSmooth_UnpackDepthProgram;
        // shaderProgram.m_textures.push_back({BGFX_INVALID_HANDLE,resolveRenderImage(m_gBufferPositionImage)->GetHandle(), 0});
        // shaderProgram.m_textures.push_back({BGFX_INVALID_HANDLE,resolveRenderImage(m_gBufferNormalImage)->GetHandle(), 0});

        GraphicsContext::LayoutStream layout;
        context.Quad(layout, vQuadPos, cVector2f(1, 1), cVector2f(0, 0), cVector2f(1, 1));

        GraphicsContext::DrawRequest drawRequest{ layout, shaderProgram };
        context.Submit(edgeSmoothView, drawRequest);
    }

    void rendering::detail::RenderZPassObject(
        bgfx::ViewId view, GraphicsContext& context, cViewport& viewport, iRenderer* renderer, iRenderable* object, Cull cull) {
        eMaterialRenderMode renderMode = object->GetCoverageAmount() >= 1 ? eMaterialRenderMode_Z : eMaterialRenderMode_Z_Dissolve;
        cMaterial* pMaterial = object->GetMaterial();
        iMaterialType* materialType = pMaterial->GetType();
        iVertexBuffer* vertexBuffer = object->GetVertexBuffer();
        if (vertexBuffer == nullptr || materialType == nullptr) {
            return;
        }

        GraphicsContext::LayoutStream layoutInput;
        vertexBuffer->GetLayoutStream(layoutInput);
        materialType->ResolveShaderProgram(
            renderMode, viewport, pMaterial, object, renderer, [&](GraphicsContext::ShaderProgram& shaderInput) {
                shaderInput.m_configuration.m_write = Write::Depth;
                shaderInput.m_configuration.m_cull = cull;
                shaderInput.m_configuration.m_depthTest = DepthTest::LessEqual;

                shaderInput.m_modelTransform =
                    object->GetModelMatrixPtr() ? object->GetModelMatrixPtr()->GetTranspose() : cMatrixf::Identity;

                GraphicsContext::DrawRequest drawRequest{ layoutInput, shaderInput };
                context.Submit(view, drawRequest);
            });
    }

    void cRendererDeferred::Draw(
        const ForgeRenderer::Frame& frame,
        cViewport& viewport,
        float afFrameTime,
        cFrustum* apFrustum,
        cWorld* apWorld,
        cRenderSettings* apSettings,
        bool abSendFrameBufferToPostEffects) {
        iRenderer::Draw(frame, viewport, afFrameTime, apFrustum, apWorld, apSettings, abSendFrameBufferToPostEffects);
        // keep around for the moment ...
        BeginRendering(afFrameTime, apFrustum, apWorld, apSettings, abSendFrameBufferToPostEffects);

        mpCurrentRenderList->Setup(mfCurrentFrameTime, apFrustum);

        const cMatrixf mainFrustumViewInv = cMath::MatrixInverse(apFrustum->GetViewMatrix());
        const cMatrixf mainFrustumView = apFrustum->GetViewMatrix();
        const cMatrixf mainFrustumProj = apFrustum->GetProjectionMatrix();
        detail::PerObjectOption perObjectOptions = {
            .m_viewMat = mainFrustumView,
            .m_projectionMat = mainFrustumProj,
        };
        
        {
            BufferUpdateDesc updatePerFrameConstantsDesc = { m_perFrameBuffer.m_handle, frame.m_frameIndex * sizeof(PerFrameData), sizeof(PerFrameData)};
            beginUpdateResource(&updatePerFrameConstantsDesc);
            reinterpret_cast<PerFrameData*>(updatePerFrameConstantsDesc.pMappedData)->m_viewMatrix = cMath::ToForgeMat(mainFrustumView);
            reinterpret_cast<PerFrameData*>(updatePerFrameConstantsDesc.pMappedData)->m_projectionMatrix = cMath::ToForgeMat(mainFrustumProj);
            reinterpret_cast<PerFrameData*>(updatePerFrameConstantsDesc.pMappedData)->m_viewProjectionMatrix = cMath::ToForgeMat(cMath::MatrixMul(mainFrustumProj, mainFrustumView));
		    endUpdateResource(&updatePerFrameConstantsDesc, NULL);
        }
		

        auto& swapChainImage = frame.m_swapChain->ppRenderTargets[frame.m_swapChainIndex];

        auto& sharedData = m_boundViewportData.resolve(viewport);
        auto& currentGBuffer = sharedData.m_gBuffer[frame.m_frameIndex];


        // {
        //     LoadActionsDesc loadActions = {};
        //     loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
        //     loadActions.mLoadActionsColor[1] = LOAD_ACTION_CLEAR;
        //     loadActions.mLoadActionsColor[2] = LOAD_ACTION_CLEAR;
        //     loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
        //     loadActions.mClearColorValues[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
        //     loadActions.mClearColorValues[1] = { 0.0f, 0.0f, 0.0f, 0.0f };
        //     loadActions.mClearColorValues[2] = { 0.0f, 0.0f, 0.0f, 0.0f };
		//     loadActions.mClearDepth = currentGBuffer.m_depthBuffer.m_handle->mClearValue;
        //     std::array targets = {
        //         currentGBuffer.m_colorBuffer.m_handle,
        //         currentGBuffer.m_normalBuffer.m_handle,
        //         currentGBuffer.m_positionBuffer.m_handle
        //     };
        //     cmdBindRenderTargets(frame.m_cmd, targets.size(), targets.data(), currentGBuffer.m_depthBuffer.m_handle, &loadActions, NULL, NULL, -1, -1);
        // }
		

        // Setup far plane coordinates
        m_farPlane = apFrustum->GetFarPlane();
        m_farTop = -tan(apFrustum->GetFOV() * 0.5f) * m_farPlane;
        m_farBottom = -m_farTop;
        m_farRight = m_farBottom * apFrustum->GetAspect();
        m_farLeft = -m_farRight;

        // auto createStandardViewConfig = [&](LegacyRenderTarget& rt) -> GraphicsContext::ViewConfiguration {
        //     GraphicsContext::ViewConfiguration viewConfig{ rt };
        //     viewConfig.m_projection = mainFrustumProj;
        //     viewConfig.m_view = mainFrustumView;
        //     viewConfig.m_viewRect = { 0, 0, sharedData.m_size.x, sharedData.m_size.y };
        //     return viewConfig;
        // };

        // [&] {
        //     GraphicsContext::ViewConfiguration viewConfig{ sharedData.m_gBuffer.m_fullTarget };
        //     viewConfig.m_viewRect = { 0, 0, sharedData.m_size.x, sharedData.m_size.y };
        //     viewConfig.m_clear = { 0, 1, 0, ClearOp::Depth | ClearOp::Stencil | ClearOp::Color };
        //     bgfx::touch(context.StartPass("Clear Depth", viewConfig));
        // }();

        // tRenderableFlag lVisibleFlags =
        //     (mpCurrentSettings->mbIsReflection) ? eRenderableFlag_VisibleInReflection : eRenderableFlag_VisibleInNonReflection;


        ///////////////////////////
        // Occlusion testing
        {
            LoadActionsDesc loadActions = {};
            loadActions.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
            loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
            loadActions.mLoadActionStencil = LOAD_ACTION_CLEAR;
            loadActions.mClearDepth = {.depth = 1.0f, .stencil = 0};
            
            ASSERT(currentGBuffer.m_depthBuffer.m_handle && "Depth buffer not created");
    
            cmdBeginDebugMarker(frame.m_cmd, 0, 1, 0, "Occlusion Testing");
            cmdBindRenderTargets(frame.m_cmd, 0, NULL, currentGBuffer.m_depthBuffer.m_handle, &loadActions, NULL, NULL, -1, -1);    
            cmdSetViewport(frame.m_cmd, 0.0f, 0.0f, (float)sharedData.m_size.x, (float)sharedData.m_size.y, 0.0f, 1.0f);
            cmdSetScissor(frame.m_cmd, 0, 0, sharedData.m_size.x, sharedData.m_size.y);
            cmdBindPipeline(frame.m_cmd, m_zPassPipeline);

            cmdBindDescriptorSet(frame.m_cmd, 0, m_zDescriptorSet.m_constSet);
            cmdBindDescriptorSet(frame.m_cmd, frame.m_frameIndex, m_zDescriptorSet.m_frameSet);
            
            detail::UpdateRenderableList(
                mpCurrentRenderList, 
                mpCurrentSettings->mpVisibleNodeTracker,
                apFrustum, mpCurrentWorld,
                eObjectVariabilityFlag_All, 
                mvCurrentOcclusionPlanes, 
                eRenderableFlag_VisibleInNonReflection, [&](iRenderable* renderable) {
                eMaterialRenderMode renderMode = renderable->GetCoverageAmount() >= 1 ? eMaterialRenderMode_Z : eMaterialRenderMode_Z_Dissolve;
                cMaterial* pMaterial = renderable->GetMaterial();
                iVertexBuffer* vertexBuffer = renderable->GetVertexBuffer();
                if (vertexBuffer == nullptr) {
                    return;
                }

                static constexpr eMaterialTexture textures[] = {
                    eMaterialTexture_Diffuse,
                    eMaterialTexture_DissolveAlpha
                };
		        detail::cmdBindMaterialDescriptor(frame, pMaterial, textures, m_zDescriptorSet.m_materialSet);
                detail::cmdBindObjectDescriptor(frame, m_zObjectIndex, apFrustum, pMaterial, renderable, m_zDescriptorSet.m_perObjectSet, perObjectOptions);

                std::array targets = {
                    eVertexBufferElement_Position,
                    eVertexBufferElement_Texture0
                };
                LegacyVertexBuffer::GeometryBinding binding{};
                static_cast<LegacyVertexBuffer*>(vertexBuffer)->resolveGeometryBinding(
                    frame.m_currentFrame, targets, &binding);
                detail::cmdDefaultLegacyGeomBinding(frame, binding);
                cmdDrawIndexed(frame.m_cmd, binding.m_indexBuffer.numIndicies, 0, 0);
            });
            cmdEndDebugMarker(frame.m_cmd);
            // auto occlusionConfig = createStandardViewConfig(sharedData.m_gBuffer.m_depthTarget);
            // auto occlusionPass = context.StartPass("Render Occlusion", occlusionConfig);

            // for(auto& object: mpCurrentRenderList->GetOcclusionQueryItems()) {
            //     object->ResolveOcclusionPass(
            //     this,
            //     [&](bgfx::OcclusionQueryHandle handle,
            //         DepthTest depth,
            //         GraphicsContext::LayoutStream& layoutStream,
            //         const cMatrixf& transformMatrix)
            //     {
            //         GraphicsContext::ShaderProgram shaderProgram;
            //         shaderProgram.m_handle = m_nullShader;
            //         shaderProgram.m_configuration.m_depthTest = depth;
            //         shaderProgram.m_configuration.m_cull = Cull::CounterClockwise;

            //         shaderProgram.m_modelTransform = transformMatrix;

            //         GraphicsContext::DrawRequest drawRequest{ layoutStream, shaderProgram };
            //         context.Submit(occlusionPass, drawRequest, handle);
            //     });
            // }

            mpCurrentRenderList->Compile(
                eRenderListCompileFlag_Diffuse | 
                eRenderListCompileFlag_Translucent | 
                eRenderListCompileFlag_Decal |
                eRenderListCompileFlag_Illumination | 
                eRenderListCompileFlag_FogArea);
        }


        {
            cmdBindRenderTargets(frame.m_cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
            std::array rtBarriers = {
                RenderTargetBarrier{ currentGBuffer.m_colorBuffer.m_handle, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                RenderTargetBarrier{ currentGBuffer.m_normalBuffer.m_handle, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                RenderTargetBarrier{ currentGBuffer.m_positionBuffer.m_handle, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                RenderTargetBarrier{ currentGBuffer.m_specularBuffer.m_handle, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
            };
            cmdResourceBarrier(frame.m_cmd, 0, NULL, 0, NULL, rtBarriers.size(), rtBarriers.data());

            cmdBeginDebugMarker(frame.m_cmd, 0, 1, 0, "GBuffer Pass");
                
            LoadActionsDesc loadActions = {};
            loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
            loadActions.mLoadActionsColor[1] = LOAD_ACTION_CLEAR;
            loadActions.mLoadActionsColor[2] = LOAD_ACTION_CLEAR;
            loadActions.mLoadActionsColor[3] = LOAD_ACTION_CLEAR;
            loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;
            std::array targets = {
                currentGBuffer.m_colorBuffer.m_handle,
                currentGBuffer.m_normalBuffer.m_handle,
                currentGBuffer.m_positionBuffer.m_handle,
                currentGBuffer.m_specularBuffer.m_handle
            };
            cmdBindRenderTargets(frame.m_cmd, targets.size(), targets.data(), currentGBuffer.m_depthBuffer.m_handle, &loadActions, NULL, NULL, -1, -1);    
            cmdSetViewport(frame.m_cmd, 0.0f, 0.0f, (float)sharedData.m_size.x, (float)sharedData.m_size.y, 0.0f, 1.0f);
            cmdSetScissor(frame.m_cmd, 0, 0, sharedData.m_size.x, sharedData.m_size.y);
            cmdBindPipeline(frame.m_cmd, m_solidDiffuseParallaxPipeline);

            cmdBindDescriptorSet(frame.m_cmd, 0, m_solidDescriptorSet.m_constSet);
            cmdBindDescriptorSet(frame.m_cmd, frame.m_frameIndex, m_solidDescriptorSet.m_frameSet);

            for(auto& diffuseItems: mpCurrentRenderList->GetRenderableItems(eRenderListType_Diffuse)) {
                cMaterial* pMaterial = diffuseItems->GetMaterial();
                iVertexBuffer* vertexBuffer = diffuseItems->GetVertexBuffer();
                if(pMaterial == nullptr || vertexBuffer == nullptr) {
                    continue;
                }

                static constexpr eMaterialTexture textures[] = {
                    eMaterialTexture_Diffuse,
                    eMaterialTexture_Height,
                    eMaterialTexture_NMap,
                    eMaterialTexture_CubeMap,
                    eMaterialTexture_Specular,
                    eMaterialTexture_CubeMapAlpha
                };

		        detail::cmdBindMaterialDescriptor(frame, pMaterial, textures, m_solidDescriptorSet.m_materialSet);
                detail::cmdBindObjectDescriptor(frame, m_solidObjectIndex, apFrustum, pMaterial, diffuseItems, m_solidDescriptorSet.m_perObjectSet, perObjectOptions);

                std::array targets = {
                    eVertexBufferElement_Position,
                    eVertexBufferElement_Texture0,
                    eVertexBufferElement_Normal,
                    eVertexBufferElement_Texture1Tangent
                };
                LegacyVertexBuffer::GeometryBinding binding;
                static_cast<LegacyVertexBuffer*>(vertexBuffer)->resolveGeometryBinding(
                    frame.m_currentFrame, targets, &binding);
                detail::cmdDefaultLegacyGeomBinding(frame, binding);
                cmdDrawIndexed(frame.m_cmd, binding.m_indexBuffer.numIndicies, 0, 0);
            }
            cmdEndDebugMarker(frame.m_cmd);
        }

        cmdBindRenderTargets(frame.m_cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
        std::array rtBarriers = {
            RenderTargetBarrier{ currentGBuffer.m_colorBuffer.m_handle, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
            RenderTargetBarrier{ currentGBuffer.m_normalBuffer.m_handle, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
            RenderTargetBarrier{ currentGBuffer.m_positionBuffer.m_handle, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
            RenderTargetBarrier{ currentGBuffer.m_specularBuffer.m_handle, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
           RenderTargetBarrier{currentGBuffer.m_outputBuffer.m_handle, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET }
        };
        cmdResourceBarrier(frame.m_cmd, 0, NULL, 0, NULL, rtBarriers.size(), rtBarriers.data());



        // // ------------------------------------------------------------------------------------
        // //  Render Diffuse Pass render to color and depth
        // // ------------------------------------------------------------------------------------
        // {
        //     detail::GBufferPassOptions args = {
        //         .projectionFrustum = mainFrustumProj,
        //         .viewFrustum = mainFrustumView,
        //         .buffer = sharedData.m_gBuffer,
        //         .name = "Gbuffer",
        //     };
        //     detail::RenderGBufferPass(
        //         context, 
        //         this,  
        //         mpCurrentRenderList->GetRenderableItems(eRenderListType_Diffuse), viewport, args);
        // }

        // // ------------------------------------------------------------------------------------
        // //  Render Decal Pass render to color and depth
        // // ------------------------------------------------------------------------------------
        // {

        //     detail::DecalPassOptions args = {
        //         .projectionFrustum = mainFrustumProj,
        //         .viewFrustum = mainFrustumView,
        //         .buffer = sharedData.m_gBuffer,
        //         .name = "Decal",
        //     };
        //     detail::RenderDecalPass(
        //         context, 
        //         this,  
        //         mpCurrentRenderList->GetRenderableItems(eRenderListType_Decal), viewport, args);
        // }

        
        // // ------------------------------------------------------------------------------------
        // //  Render SSAO Pass to color
        // // ------------------------------------------------------------------------------------
        // if(mpCurrentSettings->mbSSAOActive) {
        //     GraphicsContext::LayoutStream layoutStream;
        //     cMatrixf projMtx;
        //     context.ScreenSpaceQuad(layoutStream, projMtx, sharedData.m_size.x, sharedData.m_size.y);

        //     auto& ssaoImage = sharedData.m_gBuffer.m_SSAOImage;
        //     auto& ssaoBlurImage = sharedData.m_gBuffer.m_SSAOBlurImage;
        //     auto imageSize = ssaoImage->GetImageSize();
        //     {
        //         GraphicsContext::ViewConfiguration viewConfig{ sharedData.m_gBuffer.m_SSAOTarget };
        //         viewConfig.m_projection = projMtx;
        //         viewConfig.m_viewRect = { 0, 0, sharedData.m_size.x, sharedData.m_size.y };
        //         auto view = context.StartPass("SSAO", viewConfig);

        //         GraphicsContext::ShaderProgram shaderProgram;
        //         shaderProgram.m_handle = m_deferredSSAOProgram;

        //         struct {
        //             float u_inputRTSize[2];
        //             float u_negInvFarPlane;
        //             float u_farPlane;

        //             float depthDiffMul;
        //             float scatterDepthMul;
        //             float scatterLengthMin;
        //             float scatterLengthMax;
        //         } u_param = {{0}};
        //         u_param.u_inputRTSize[0] = imageSize.x;
        //         u_param.u_inputRTSize[1] = imageSize.y;
        //         u_param.u_negInvFarPlane = -1.0f / m_farPlane;
        //         u_param.u_farPlane = m_farPlane;

        //         u_param.depthDiffMul = mfSSAODepthDiffMul;
        //         u_param.scatterDepthMul = mfSSAOScatterLengthMul;
        //         u_param.scatterLengthMin = mfSSAOScatterLengthMin;
        //         u_param.scatterLengthMax = mfSSAOScatterLengthMax;

        //         shaderProgram.m_uniforms.push_back({ m_u_param, &u_param, 2 });

        //         shaderProgram.m_textures.push_back({ m_s_positionMap, sharedData.m_gBuffer.m_positionImage->GetHandle(), 0 });
        //         shaderProgram.m_textures.push_back({ m_s_normalMap, sharedData.m_gBuffer.m_normalImage->GetHandle(), 1});
        //         shaderProgram.m_textures.push_back({ m_s_scatterDisk, m_ssaoScatterDiskImage->GetHandle(), 2 });
                
        //         shaderProgram.m_configuration.m_write = Write::R;
        //         shaderProgram.m_configuration.m_rgbBlendFunc =
        //             CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::Zero);
        //         shaderProgram.m_configuration.m_alphaBlendFunc =
        //             CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::Zero);

        //         GraphicsContext::DrawRequest request{ layoutStream, shaderProgram };
        //         context.Submit(view, request);
        //     }
        //     // blur pass horizontal
        //     {
        //         GraphicsContext::ViewConfiguration viewConfig{ sharedData.m_gBuffer.m_SSAOBlurTarget };
        //         viewConfig.m_projection = projMtx;
        //         viewConfig.m_viewRect = { 0, 0, ssaoBlurImage->GetWidth(), ssaoBlurImage->GetHeight() };
        //         auto view = context.StartPass("SSAO Horizontal", viewConfig);

        //         GraphicsContext::ShaderProgram shaderProgram;
        //         shaderProgram.m_handle = m_deferredSSAOBlurHorizontalProgram;

        //         shaderProgram.m_configuration.m_write = Write::R;
        //         shaderProgram.m_configuration.m_rgbBlendFunc =
        //             CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::Zero);
        //         shaderProgram.m_configuration.m_alphaBlendFunc =
        //             CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::Zero);

        //         shaderProgram.m_textures.push_back({ m_s_diffuseMap, sharedData.m_gBuffer.m_SSAOImage->GetHandle(), 0 });
        //         shaderProgram.m_textures.push_back({ m_s_depthMap, sharedData.m_gBuffer.m_depthStencilImage->GetHandle(), 1});
        //         struct {
        //             float u_farPlane;
        //             float pad[3];
        //         } u_param = {{0}};
        //         u_param.u_farPlane = m_farPlane;
        //         shaderProgram.m_uniforms.push_back({ m_u_param, &u_param, 1 });

        //         GraphicsContext::DrawRequest request{ layoutStream, shaderProgram };
        //         context.Submit(view, request);
        //     }

        //      // blur pass horizontal
        //     {
        //         GraphicsContext::ViewConfiguration viewConfig{ sharedData.m_gBuffer.m_colorImage };
        //         viewConfig.m_projection = projMtx;
        //         viewConfig.m_viewRect = { 0, 0, sharedData.m_size.x, sharedData.m_size.y };
        //         auto view = context.StartPass("SSAO Vertical", viewConfig);

        //         GraphicsContext::ShaderProgram shaderProgram;
        //         shaderProgram.m_handle = m_deferredSSAOBlurVerticalProgram;

        //         shaderProgram.m_configuration.m_write = Write::RGB;
        //         shaderProgram.m_configuration.m_rgbBlendFunc =
        //             CreateBlendFunction(BlendOperator::Add, BlendOperand::Zero, BlendOperand::SrcColor);
        //         shaderProgram.m_configuration.m_alphaBlendFunc =
        //             CreateBlendFunction(BlendOperator::Add, BlendOperand::Zero, BlendOperand::One);

        //         shaderProgram.m_textures.push_back({ m_s_diffuseMap, sharedData.m_gBuffer.m_SSAOBlurImage->GetHandle(), 0 });
        //         shaderProgram.m_textures.push_back({ m_s_depthMap, sharedData.m_gBuffer.m_depthStencilImage->GetHandle(), 1});
        //         struct {
        //             float u_farPlane;
        //             float pad[3];
        //         } u_param = {{0}};
        //         u_param.u_farPlane = m_farPlane;
        //         shaderProgram.m_uniforms.push_back({ m_u_param, &u_param, 1 });

        //         GraphicsContext::DrawRequest request{ layoutStream, shaderProgram };
        //         context.Submit(view, request);
        //     }

        // }

        {
            cVector2l screenSize = viewport.GetSize();

            float fScreenArea = (float)(screenSize.x * screenSize.y);
            int mlMinLargeLightArea = (int)(MinLargeLightNormalizedArea * fScreenArea);

            // std::array<std::vector<detail::DeferredLight*>, eDeferredLightList_LastEnum> sortedLights;
            // DON'T touch deferredLights after this point
            // auto lightSpan = lights;
            std::vector<detail::DeferredLight> deferredLights;
            deferredLights.reserve(mpCurrentRenderList->GetLights().size());
            std::vector<detail::DeferredLight*> deferredLightBoxRenderBack;
            std::vector<detail::DeferredLight*> deferredLightBoxStencilFront;

            std::vector<detail::DeferredLight*> deferredLightRenderBack;
            std::vector<detail::DeferredLight*> deferredLightStencilFrontRenderBack;

            for (auto& light : mpCurrentRenderList->GetLights()) {
                auto lightType = light->GetLightType();
                auto& deferredLightData = deferredLights.emplace_back(detail::DeferredLight());
                deferredLightData.m_light = light;
                if (lightType == eLightType_Box) {
                    continue;
                }
                switch (lightType) {
                case eLightType_Point:
                    {
                        deferredLightData.m_insideNearPlane = apFrustum->CheckSphereNearPlaneIntersection(
                            light->GetWorldPosition(), light->GetRadius() * kLightRadiusMul_Low);
                        detail::SetupLightMatrix(
                            deferredLightData.m_mtxViewSpaceRender,
                            deferredLightData.m_mtxViewSpaceTransform,
                            light,
                            apFrustum,
                            kLightRadiusMul_Medium);
                        deferredLightData.m_clipRect = cMath::GetClipRectFromSphere(
                            deferredLightData.m_mtxViewSpaceRender.GetTranslation(), light->GetRadius(), apFrustum, screenSize, true, 0);
                        break;
                    }
                case eLightType_Spot:
                    {
                        cLightSpot* lightSpot = static_cast<cLightSpot*>(light);
                        deferredLightData.m_insideNearPlane = apFrustum->CheckFrustumNearPlaneIntersection(lightSpot->GetFrustum());
                        detail::SetupLightMatrix(
                            deferredLightData.m_mtxViewSpaceRender,
                            deferredLightData.m_mtxViewSpaceTransform,
                            light,
                            apFrustum,
                            kLightRadiusMul_Medium);
                        cMath::GetClipRectFromBV(deferredLightData.m_clipRect, *light->GetBoundingVolume(), apFrustum, screenSize, 0);
                        break;
                    }
                default:
                    break;
                }

                if (lightType == eLightType_Spot && light->GetCastShadows() && mpCurrentSettings->mbRenderShadows) {
                    cLightSpot* pLightSpot = static_cast<cLightSpot*>(light);

                    ////////////////////////
                    // Inside near plane, use max resolution
                    if (deferredLightData.m_insideNearPlane) {
                        deferredLightData.m_castShadows = true;

                        deferredLightData.m_shadowResolution = rendering::detail::GetShadowMapResolution(
                            light->GetShadowMapResolution(), mpCurrentSettings->mMaxShadowMapResolution);
                    } else {
                        cVector3f vIntersection = pLightSpot->GetFrustum()->GetOrigin();
                        pLightSpot->GetFrustum()->CheckLineIntersection(
                            apFrustum->GetOrigin(), light->GetBoundingVolume()->GetWorldCenter(), vIntersection);

                        float fDistToLight = cMath::Vector3Dist(apFrustum->GetOrigin(), vIntersection);

                        deferredLightData.m_castShadows = true;
                        deferredLightData.m_shadowResolution = rendering::detail::GetShadowMapResolution(
                            light->GetShadowMapResolution(), mpCurrentSettings->mMaxShadowMapResolution);

                        ///////////////////////
                        // Skip shadow
                        if (fDistToLight > m_shadowDistanceNone) {
                            deferredLightData.m_castShadows = false;
                        }
                        ///////////////////////
                        // Use Low
                        else if (fDistToLight > m_shadowDistanceLow) {
                            if (deferredLightData.m_shadowResolution == eShadowMapResolution_Low) {
                                deferredLightData.m_castShadows = false;
                            }
                            deferredLightData.m_shadowResolution = eShadowMapResolution_Low;
                        }
                        ///////////////////////
                        // Use Medium
                        else if (fDistToLight > m_shadowDistanceMedium) {
                            if (deferredLightData.m_shadowResolution == eShadowMapResolution_High) {
                                deferredLightData.m_shadowResolution = eShadowMapResolution_Medium;
                            } else {
                                deferredLightData.m_shadowResolution = eShadowMapResolution_Low;
                            }
                        }
                    }
                }
            }

            mpCurrentSettings->mlNumberOfLightsRendered = 0;
            for (auto& deferredLight : deferredLights) {
                // cDeferredLight* pLightData =  mvTempDeferredLights[i];
                iLight* pLight = deferredLight.m_light;
                eLightType lightType = pLight->GetLightType();

                ////////////////////////
                // If box, we have special case...
                if (lightType == eLightType_Box) {
                    cLightBox* pLightBox = static_cast<cLightBox*>(pLight);

                    // Set up matrix
                    deferredLight.m_mtxViewSpaceRender = cMath::MatrixScale(pLightBox->GetSize());
                    deferredLight.m_mtxViewSpaceRender.SetTranslation(pLightBox->GetWorldPosition());
                    deferredLight.m_mtxViewSpaceRender = cMath::MatrixMul(apFrustum->GetViewMatrix(), deferredLight.m_mtxViewSpaceRender);

                    mpCurrentSettings->mlNumberOfLightsRendered++;

                    // Check if near plane is inside box. If so only render back
                    if (apFrustum->CheckBVNearPlaneIntersection(pLight->GetBoundingVolume())) {
                        deferredLightBoxRenderBack.emplace_back(&deferredLight);
                    } else {
                        deferredLightBoxStencilFront.emplace_back(&deferredLight);
                    }

                    continue;
                }

                mpCurrentSettings->mlNumberOfLightsRendered++;

                if (deferredLight.m_insideNearPlane) {
                    deferredLightRenderBack.emplace_back(&deferredLight);
                } else {
                    if (lightType == eLightType_Point) {
                        if (deferredLight.getArea() >= mlMinLargeLightArea) {
                            deferredLightStencilFrontRenderBack.emplace_back(&deferredLight);
                        } else {
                            deferredLightRenderBack.emplace_back(&deferredLight);
                        }
                    }
                    // Always do double passes for spotlights as they need to will get artefacts otherwise...
                    //(At least with gobos)l
                    else if (lightType == eLightType_Spot) {
                        deferredLightStencilFrontRenderBack.emplace_back(&deferredLight);
                    }
                }
            }
            std::sort(deferredLightBoxRenderBack.begin(), deferredLightBoxRenderBack.end(), detail::SortDeferredLightBox);
            std::sort(deferredLightBoxStencilFront.begin(), deferredLightBoxStencilFront.end(), detail::SortDeferredLightBox);
            std::sort(deferredLightRenderBack.begin(), deferredLightRenderBack.end(), detail::SortDeferredLightDefault);
            std::sort(deferredLightStencilFrontRenderBack.begin(), deferredLightStencilFrontRenderBack.end(), detail::SortDeferredLightDefault);

            // {
            //     GraphicsContext::ViewConfiguration clearBackBufferView{ options.m_gBuffer.m_outputTarget };
            //     clearBackBufferView.m_clear = { 0, 1, 0, ClearOp::Color };
            //     clearBackBufferView.m_viewRect = {
            //         0, 0, static_cast<uint16_t>(screenSize.x), static_cast<uint16_t>(screenSize.y)
            //     };
            //     bgfx::touch(context.StartPass("Clear Backbuffer", clearBackBufferView));
            // }
            // -----------------------------------------------
            // Draw Box Lights
            // -----------------------------------------------
            // {
            //     auto drawBoxLight = [&](bgfx::ViewId view, GraphicsContext::ShaderProgram& shaderProgram, detail::DeferredLight* light) {
            //         GraphicsContext::LayoutStream layoutStream;
            //         mpShapeBox->GetLayoutStream(layoutStream);
            //         cLightBox* pLightBox = static_cast<cLightBox*>(light->m_light);

            //         const auto& color = light->m_light->GetDiffuseColor();
            //         float lightColor[4] = { color.r, color.g, color.b, color.a };
            //         shaderProgram.m_handle = m_lightBoxProgram;
            //         shaderProgram.m_textures.push_back({ m_s_diffuseMap, options.m_gBuffer.m_colorImage->GetHandle(), 0 });
            //         shaderProgram.m_uniforms.push_back({ m_u_lightColor, lightColor });

            //        shaderProgram.m_modelTransform = detail::GetLightMtx(*light).GetTranspose();

            //         switch (pLightBox->GetBlendFunc()) {
            //         case eLightBoxBlendFunc_Add:
            //             shaderProgram.m_configuration.m_rgbBlendFunc =
            //                 CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
            //             shaderProgram.m_configuration.m_alphaBlendFunc =
            //                 CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
            //             break;
            //         case eLightBoxBlendFunc_Replace:
            //             break;
            //         default:
            //             BX_ASSERT(false, "Unknown blend func %d", pLightBox->GetBlendFunc());
            //             break;
            //         }

            //         GraphicsContext::DrawRequest drawRequest{ layoutStream, shaderProgram };
            //         context.Submit(view, drawRequest);
            //     };

            //     GraphicsContext::ViewConfiguration stencilFrontBackViewConfig{ options.m_gBuffer.m_outputTarget };
            //     stencilFrontBackViewConfig.m_projection = options.frustumProjection;
            //     stencilFrontBackViewConfig.m_view = options.frustumView;
            //     stencilFrontBackViewConfig.m_clear = { 0, 1.0, 0, ClearOp::Stencil };
            //     stencilFrontBackViewConfig.m_viewRect = {
            //         0, 0, static_cast<uint16_t>(screenSize.x), static_cast<uint16_t>(screenSize.y)
            //     };
            //     const auto boxStencilPass = context.StartPass("eDeferredLightList_Box_StencilFront_RenderBack",
            //     stencilFrontBackViewConfig); bgfx::setViewMode(boxStencilPass, bgfx::ViewMode::Sequential);

            //     for (auto& light : deferredLightBoxStencilFront) {
            //         {
            //             GraphicsContext::ShaderProgram shaderProgram;
            //             GraphicsContext::LayoutStream layoutStream;
            //             mpShapeBox->GetLayoutStream(layoutStream);

            //             shaderProgram.m_handle = m_nullShader;
            //             shaderProgram.m_configuration.m_cull = Cull::CounterClockwise;
            //             shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
            //             shaderProgram.m_configuration.m_frontStencilTest = CreateStencilTest(
            //                 StencilFunction::Always, StencilFail::Keep, StencilDepthFail::Replace, StencilDepthPass::Keep, 0xff, 0xff);

            //             shaderProgram.m_modelTransform = detail::GetLightMtx(*light).GetTranspose();

            //             GraphicsContext::DrawRequest drawRequest{ layoutStream, shaderProgram };
            //             // drawRequest.m_clear =  GraphicsContext::ClearRequest{0, 0, 0, ClearOp::Stencil};
            //             context.Submit(boxStencilPass, drawRequest);
            //         }

            //         {
            //             GraphicsContext::ShaderProgram shaderProgram;
            //             shaderProgram.m_configuration.m_cull = Cull::Clockwise;
            //             shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
            //             shaderProgram.m_configuration.m_write = Write::RGB;
            //             shaderProgram.m_configuration.m_frontStencilTest = CreateStencilTest(
            //                 StencilFunction::Equal, StencilFail::Zero, StencilDepthFail::Zero, StencilDepthPass::Zero, 0xff, 0xff);
            //             drawBoxLight(boxStencilPass, shaderProgram, light);
            //         }
            //     }

            //     GraphicsContext::ViewConfiguration boxLightConfig{ options.m_gBuffer.m_outputTarget };
            //     boxLightConfig.m_projection = options.frustumProjection;
            //     boxLightConfig.m_view = options.frustumView;
            //     boxLightConfig.m_viewRect = { 0, 0, static_cast<uint16_t>(screenSize.x), static_cast<uint16_t>(screenSize.y) };
            //     const auto boxLightBackPass = context.StartPass("eDeferredLightList_Box_RenderBack", boxLightConfig);
            //     for (auto& light : deferredLightBoxRenderBack) {
            //         GraphicsContext::ShaderProgram shaderProgram;
            //         shaderProgram.m_configuration.m_cull = Cull::Clockwise;
            //         shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
            //         shaderProgram.m_configuration.m_write = Write::RGB;
            //         drawBoxLight(boxLightBackPass, shaderProgram, light);
            //     }
            // }

            // -----------------------------------------------
            // Draw Point Lights
            // Draw Spot Lights
            // -----------------------------------------------
            {
                // auto drawLight = [&](bgfx::ViewId pass, GraphicsContext::ShaderProgram& shaderProgram, detail::DeferredLight*
                // apLightData) {
                //     GraphicsContext::LayoutStream layoutStream;
                //     GetLightShape(apLightData->m_light, eDeferredShapeQuality_High)->GetLayoutStream(layoutStream);
                //     GraphicsContext::DrawRequest drawRequest{ layoutStream, shaderProgram };
                //     switch (apLightData->m_light->GetLightType()) {
                //     case eLightType_Point:
                //         {
                //             struct {
                //                 float lightRadius;
                //                 float pad[3];
                //             } param = { 0 };
                //             param.lightRadius = apLightData->m_light->GetRadius();
                //             auto attenuationImage = apLightData->m_light->GetFalloffMap();

                //             const auto modelViewMtx = cMath::MatrixMul(apFrustum->GetViewMatrix(),
                //             apLightData->m_light->GetWorldMatrix()); const auto color = apLightData->m_light->GetDiffuseColor();
                //             cVector3f lightViewPos = cMath::MatrixMul(modelViewMtx, detail::GetLightMtx(*apLightData)).GetTranslation();
                //             float lightPosition[4] = { lightViewPos.x, lightViewPos.y, lightViewPos.z, 1.0f };
                //             float lightColor[4] = { color.r, color.g, color.b, color.a };
                //             cMatrixf mtxInvViewRotation =
                //                 cMath::MatrixMul(apLightData->m_light->GetWorldMatrix(), options.frustumInvView).GetTranspose();

                //             shaderProgram.m_uniforms.push_back({ m_u_lightPos, lightPosition });
                //             shaderProgram.m_uniforms.push_back({ m_u_lightColor, lightColor });
                //             shaderProgram.m_uniforms.push_back({ m_u_param, &param });
                //             shaderProgram.m_uniforms.push_back({ m_u_mtxInvViewRotation, mtxInvViewRotation.v });

                //             // shaderProgram.m_textures.push_back({ m_s_diffuseMap, options.m_gBuffer.m_colorImage->GetHandle(), 1 });
                //             // shaderProgram.m_textures.push_back({ m_s_normalMap, options.m_gBuffer.m_normalImage->GetHandle(), 2 });
                //             // shaderProgram.m_textures.push_back({ m_s_positionMap, options.m_gBuffer.m_positionImage->GetHandle(), 3
                //             });
                //             // shaderProgram.m_textures.push_back({ m_s_specularMap, options.m_gBuffer.m_specularImage->GetHandle(), 4
                //             }); shaderProgram.m_textures.push_back({ m_s_attenuationLightMap, attenuationImage->GetHandle(), 5 });

                //             uint32_t flags = 0;
                //             if (apLightData->m_light->GetGoboTexture()) {
                //                 flags |= rendering::detail::PointlightVariant_UseGoboMap;
                //                 shaderProgram.m_textures.push_back({ m_s_goboMap, apLightData->m_light->GetGoboTexture()->GetHandle(), 0
                //                 });
                //             }
                //             shaderProgram.m_handle = m_pointLightVariants.GetVariant(flags);
                //             context.Submit(pass, drawRequest);
                //             break;
                //         }
                //     case eLightType_Spot:
                //         {
                //             cLightSpot* pLightSpot = static_cast<cLightSpot*>(apLightData->m_light);
                //             // Calculate and set the forward vector
                //             cVector3f vForward = cVector3f(0, 0, 1);
                //             vForward = cMath::MatrixMul3x3(apLightData->m_mtxViewSpaceTransform, vForward);

                //             struct {
                //                 float lightRadius;
                //                 float lightForward[3];

                //                 float oneMinusCosHalfSpotFOV;
                //                 float shadowMapOffset[2];
                //                 float pad;
                //             } uParam = { apLightData->m_light->GetRadius(),
                //                          { vForward.x, vForward.y, vForward.z },
                //                          1 - pLightSpot->GetCosHalfFOV(),
                //                          { 0, 0 },
                //                          0

                //             };
                //             auto goboImage = apLightData->m_light->GetGoboTexture();
                //             auto spotFallOffImage = pLightSpot->GetSpotFalloffMap();
                //             auto spotAttenuationImage = pLightSpot->GetFalloffMap();

                //             uint32_t flags = 0;
                //             const auto modelViewMtx = cMath::MatrixMul(apFrustum->GetViewMatrix(),
                //             apLightData->m_light->GetWorldMatrix()); const auto color = apLightData->m_light->GetDiffuseColor();
                //             cVector3f lightViewPos = cMath::MatrixMul(modelViewMtx, detail::GetLightMtx(*apLightData)).GetTranslation();
                //             float lightPosition[4] = { lightViewPos.x, lightViewPos.y, lightViewPos.z, 1.0f };
                //             float lightColor[4] = { color.r, color.g, color.b, color.a };
                //             cMatrixf spotViewProj = cMath::MatrixMul(pLightSpot->GetViewProjMatrix(),
                //             options.frustumInvView).GetTranspose();

                //             shaderProgram.m_uniforms.push_back({ m_u_lightPos, lightPosition });
                //             shaderProgram.m_uniforms.push_back({ m_u_lightColor, lightColor });
                //             shaderProgram.m_uniforms.push_back({ m_u_spotViewProj, &spotViewProj.v });

                //             // shaderProgram.m_textures.push_back({ m_s_diffuseMap, options.m_gBuffer.m_colorImage->GetHandle(), 0 });
                //             // shaderProgram.m_textures.push_back({ m_s_normalMap, options.m_gBuffer.m_normalImage->GetHandle(), 1 });
                //             // shaderProgram.m_textures.push_back({ m_s_positionMap, options.m_gBuffer.m_positionImage->GetHandle(), 2
                //             });
                //             // shaderProgram.m_textures.push_back({ m_s_specularMap, options.m_gBuffer.m_specularImage->GetHandle(), 3
                //             });

                //             shaderProgram.m_textures.push_back({ m_s_attenuationLightMap, spotAttenuationImage->GetHandle(), 4 });
                //             if (goboImage) {
                //                 shaderProgram.m_textures.push_back({ m_s_goboMap, goboImage->GetHandle(), 5 });
                //                 flags |= rendering::detail::SpotlightVariant_UseGoboMap;
                //             } else {
                //                 shaderProgram.m_textures.push_back({ m_s_spotFalloffMap, spotFallOffImage->GetHandle(), 5 });
                //             }
                //             auto& currentLight = apLightData->m_light;
                //             BX_ASSERT(currentLight->GetLightType() == eLightType_Spot, "Only spot lights are supported for shadow
                //             rendering")

                //             cLightSpot* pSpotLight = static_cast<cLightSpot*>(currentLight);
                //             cFrustum* pLightFrustum = pSpotLight->GetFrustum();

                //             std::vector<iRenderable*> shadowCasters;
                //             if (apLightData->m_castShadows &&
                //                 detail::SetupShadowMapRendering(shadowCasters, apWorld, pLightFrustum, pLightSpot,
                //                 mvCurrentOcclusionPlanes)) { flags |= rendering::detail::SpotlightVariant_UseShadowMap;
                //                 eShadowMapResolution shadowMapRes = apLightData->m_shadowResolution;

                //                 auto findBestShadowMap = [&](eShadowMapResolution resolution,
                //                                              iLight* light) -> cRendererDeferred::ShadowMapData* {
                //                     auto& shadowMapVec = m_shadowMapData[resolution];
                //                     int maxFrameDistance = -1;
                //                     size_t bestIndex = 0;
                //                     for (size_t i = 0; i < shadowMapVec.size(); ++i) {
                //                         auto& shadowMap = shadowMapVec[i];
                //                         if (shadowMap.m_light == light) {
                //                             shadowMap.m_frameCount = iRenderer::GetRenderFrameCount();
                //                             return &shadowMap;
                //                         }

                //                         const int frameDist = cMath::Abs(shadowMap.m_frameCount - iRenderer::GetRenderFrameCount());
                //                         if (frameDist > maxFrameDistance) {
                //                             maxFrameDistance = frameDist;
                //                             bestIndex = i;
                //                         }
                //                     }
                //                     if (maxFrameDistance != -1) {
                //                         shadowMapVec[bestIndex].m_frameCount = iRenderer::GetRenderFrameCount();
                //                         return &shadowMapVec[bestIndex];
                //                     }
                //                     return nullptr;
                //                 };
                //                 auto* shadowMapData = findBestShadowMap(shadowMapRes, currentLight);
                //                 if (!shadowMapData) {
                //                     // No shadow map available
                //                     BX_ASSERT(false, "No shadow map available");
                //                     break;
                //                 }
                //                 const auto shadowMapSize = shadowMapData->m_target.GetImage()->GetImageSize();
                //                 // testing if the shadow map needs to be updated
                //                 if ([&]() -> bool {
                //                         // Check if texture map and light are valid
                //                         if (currentLight->GetOcclusionCullShadowCasters()) {
                //                             return true;
                //                         }

                //                         if (currentLight->GetLightType() == eLightType_Spot &&
                //                             (pSpotLight->GetAspect() != shadowMapData->m_aspect ||
                //                              pSpotLight->GetFOV() != shadowMapData->m_fov)) {
                //                             return true;
                //                         }
                //                         return !currentLight->ShadowCastersAreUnchanged(shadowCasters);
                //                     }()) {
                //                     shadowMapData->m_light = currentLight;
                //                     shadowMapData->m_transformCount = currentLight->GetTransformUpdateCount();
                //                     shadowMapData->m_radius = currentLight->GetRadius();

                //                     if (currentLight->GetLightType() == eLightType_Spot) {
                //                         shadowMapData->m_aspect = pSpotLight->GetAspect();
                //                         shadowMapData->m_fov = pSpotLight->GetFOV();
                //                     }
                //                     currentLight->SetShadowCasterCacheFromVec(shadowCasters);

                //                     GraphicsContext::ViewConfiguration shadowPassViewConfig{ shadowMapData->m_target };
                //                     shadowPassViewConfig.m_clear = { 0, 1.0, 0, ClearOp::Depth };
                //                     shadowPassViewConfig.m_view = pLightFrustum->GetViewMatrix().GetTranspose();
                //                     shadowPassViewConfig.m_projection = pLightFrustum->GetProjectionMatrix().GetTranspose();
                //                     shadowPassViewConfig.m_viewRect = {
                //                         0, 0, static_cast<uint16_t>(shadowMapSize.x), static_cast<uint16_t>(shadowMapSize.y)
                //                     };
                //                     bgfx::ViewId view = context.StartPass("Shadow Pass", shadowPassViewConfig);
                //                     for (auto& shadowCaster : shadowCasters) {
                //                         rendering::detail::RenderZPassObject(
                //                             view,
                //                             context,
                //                             viewport,
                //                             this,
                //                             shadowCaster,
                //                             pLightFrustum->GetInvertsCullMode() ? Cull::Clockwise : Cull::CounterClockwise);
                //                     }
                //                 }
                //                 uParam.shadowMapOffset[0] = 1.0f / shadowMapSize.x;
                //                 uParam.shadowMapOffset[1] = 1.0f / shadowMapSize.y;
                //                 if (m_shadowJitterImage) {
                //                     shaderProgram.m_textures.push_back({ m_s_shadowOffsetMap, m_shadowJitterImage->GetHandle(), 7 });
                //                 }
                //                 shaderProgram.m_textures.push_back({ m_s_shadowMap, shadowMapData->m_target.GetImage()->GetHandle(), 6
                //                 });
                //             }
                //             shaderProgram.m_uniforms.push_back({ m_u_param, &uParam, 2 });
                //             shaderProgram.m_handle = m_spotlightVariants.GetVariant(flags);

                //             context.Submit(pass, drawRequest);
                //             break;
                //         }
                //     default:
                //         break;
                //     }
                // };

                // GraphicsContext::ViewConfiguration viewConfig{ options.m_gBuffer.m_outputTarget };
                // viewConfig.m_viewRect = { 0, 0, screenSize.x, screenSize.y };
                // viewConfig.m_projection = options.frustumProjection;
                // viewConfig.m_view = options.frustumView;
                // const auto lightStencilBackPass = context.StartPass("eDeferredLightList_StencilFront_RenderBack", viewConfig);
                // bgfx::setViewMode(lightStencilBackPass, bgfx::ViewMode::Sequential);

                // for (auto& light : deferredLightStencilFrontRenderBack) {
                //     {
                //         GraphicsContext::ShaderProgram shaderProgram;
                //         GraphicsContext::LayoutStream layoutStream;

                //         GetLightShape(light->m_light, eDeferredShapeQuality_Medium)->GetLayoutStream(layoutStream);

                //         shaderProgram.m_handle = m_nullShader;
                //         shaderProgram.m_configuration.m_cull = Cull::CounterClockwise;
                //         shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;

                //         shaderProgram.m_modelTransform =
                //             cMath::MatrixMul(light->m_light->GetWorldMatrix(), detail::GetLightMtx(*light)).GetTranspose();

                //         shaderProgram.m_configuration.m_frontStencilTest = CreateStencilTest(
                //             StencilFunction::Always, StencilFail::Keep, StencilDepthFail::Replace, StencilDepthPass::Keep, 0xff, 0xff);

                //         GraphicsContext::DrawRequest drawRequest{ layoutStream, shaderProgram };
                //         // drawRequest.m_clear =  GraphicsContext::ClearRequest{0, 0, 0, ClearOp::Stencil};
                //         // drawRequest.m_width = mvScreenSize.x;
                //         // drawRequest.m_height = mvScreenSize.y;
                //         // if(bgfx::isValid(light->m_occlusionQuery)) {
                //         // 	bgfx::setCondition(light->m_occlusionQuery, true);
                //         // }
                //         context.Submit(lightStencilBackPass, drawRequest);
                //     }
                //     {
                //         GraphicsContext::ShaderProgram shaderProgram;
                //         shaderProgram.m_configuration.m_cull = Cull::Clockwise;
                //         shaderProgram.m_configuration.m_write = Write::RGB;
                //         shaderProgram.m_configuration.m_depthTest = DepthTest::GreaterEqual;
                //         shaderProgram.m_configuration.m_frontStencilTest = CreateStencilTest(
                //             StencilFunction::Equal, StencilFail::Zero, StencilDepthFail::Zero, StencilDepthPass::Zero, 0xff, 0xff);
                //         shaderProgram.m_configuration.m_rgbBlendFunc =
                //             CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);
                //         shaderProgram.m_configuration.m_alphaBlendFunc =
                //             CreateBlendFunction(BlendOperator::Add, BlendOperand::One, BlendOperand::One);

                //         shaderProgram.m_modelTransform =
                //             cMath::MatrixMul(light->m_light->GetWorldMatrix(), detail::GetLightMtx(*light)).GetTranspose();

                //         drawLight(lightStencilBackPass, shaderProgram, light);
                //     }
                // }

                // GraphicsContext::ViewConfiguration lightBackPassConfig{ options.m_gBuffer.m_outputTarget };
                // lightBackPassConfig.m_projection = options.frustumProjection;
                // lightBackPassConfig.m_view = options.frustumView;
                // lightBackPassConfig.m_viewRect = { 0, 0, screenSize.x, screenSize.y };
                // const auto lightBackPass = context.StartPass("eDeferredLightList_RenderBack", lightBackPassConfig);
                cmdBeginDebugMarker(frame.m_cmd, 0, 1, 0, "Point Light Deferred Back");
                
                LoadActionsDesc loadActions = {};
                loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
                loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;
                loadActions.mLoadActionStencil = LOAD_ACTION_CLEAR;
                std::array targets = {
                    currentGBuffer.m_outputBuffer.m_handle,
                };
                
                cmdBindRenderTargets(frame.m_cmd, targets.size(), targets.data(), currentGBuffer.m_depthBuffer.m_handle, &loadActions, nullptr, nullptr, -1, -1);
                cmdSetViewport(frame.m_cmd, 0.0f, 0.0f, (float)sharedData.m_size.x, (float)sharedData.m_size.y, 0.0f, 1.0f);
                cmdSetScissor(frame.m_cmd, 0, 0, sharedData.m_size.x, sharedData.m_size.y);

                {
                    DescriptorData params[15] = {};
                    size_t paramCount = 0;
                    params[paramCount].pName = "diffuseMap";
                    params[paramCount++].ppTextures = &currentGBuffer.m_colorBuffer.m_handle->pTexture;
                    params[paramCount].pName = "normalMap";
                    params[paramCount++].ppTextures = &currentGBuffer.m_normalBuffer.m_handle->pTexture;
                    params[paramCount].pName = "positionMap";
                    params[paramCount++].ppTextures = &currentGBuffer.m_positionBuffer.m_handle->pTexture;
                    params[paramCount].pName = "specularMap";
                    params[paramCount++].ppTextures = &currentGBuffer.m_specularBuffer.m_handle->pTexture;
                    updateDescriptorSet(frame.m_renderer->Rend(), frame.m_frameIndex, m_lightFrameSet, paramCount, params);
                }

                float2 viewTexel = { 1.0f / sharedData.m_size.x, 1.0f / sharedData.m_size.y };
                uint32_t rootConstantIndex = getDescriptorIndexFromName(m_lightPassRootSignature, "uRootConstants");
                cmdBindPushConstants(frame.m_cmd, m_lightPassRootSignature, rootConstantIndex, &viewTexel);
                cmdBindDescriptorSet(frame.m_cmd, frame.m_frameIndex, m_lightFrameSet);

                // updates and binds the light data
                auto cmdBindLightDescriptor = [&](detail::DeferredLight* light) {
                    DescriptorData params[10] = {};
                    size_t paramCount = 0;
                    LightUniformData uniformObjectData = {};
                    GPURingBufferOffset uniformBuffer = getGPURingBufferOffset(m_lightPassRingBuffer, sizeof(LightUniformData));

                    switch (light->m_light->GetLightType()) {
                        case eLightType_Point: {
                            const auto modelViewMtx = cMath::MatrixMul(apFrustum->GetViewMatrix(), light->m_light->GetWorldMatrix());
                            cVector3f lightViewPos = cMath::MatrixMul(modelViewMtx, detail::GetLightMtx(*light)).GetTranslation();
                            const auto color = light->m_light->GetDiffuseColor();

                            if (light->m_light->GetGoboTexture()) {
                                uniformObjectData.m_common.m_config |= LightConfiguration::HasGoboMap;
                                params[paramCount].pName = "goboMap";
                                params[paramCount++].ppTextures = &light->m_light->GetGoboTexture()->GetTexture().m_handle;
                            }

                            uniformObjectData.m_pointLight.m_radius = light->m_light->GetRadius();
                            uniformObjectData.m_pointLight.m_lightPos = float3(lightViewPos.x, lightViewPos.y, lightViewPos.z);
                            uniformObjectData.m_pointLight.m_lightColor = float4(color.r, color.g, color.b, color.a);
                            cMatrixf mtxInvViewRotation =
                                cMath::MatrixMul(light->m_light->GetWorldMatrix(), mainFrustumViewInv).GetTranspose();
                            uniformObjectData.m_pointLight.m_invViewRotation = cMath::ToForgeMat4(mtxInvViewRotation);
                            uniformObjectData.m_pointLight.m_common.m_mvp =
                                cMath::ToForgeMat4(cMath::MatrixMul(mainFrustumProj, modelViewMtx));
                            break;
                        }
                        case eLightType_Spot: {
                            break;
                        }
                        case eLightType_Box: {
                            break;
                        }
                        default: {
                            ASSERT(false && "Unsupported light type");
                            break;
                        }
                    }

                    BufferUpdateDesc updateDesc = { uniformBuffer.pBuffer, uniformBuffer.mOffset };
                    beginUpdateResource(&updateDesc);
                    memcpy(updateDesc.pMappedData, &uniformObjectData, sizeof(LightUniformData));
                    endUpdateResource(&updateDesc, NULL);

                    params[paramCount].pName = "uniformObjectBlock";
                    DescriptorDataRange range = { static_cast<uint32_t>(uniformBuffer.mOffset), sizeof(LightUniformData) };
                    params[paramCount].pRanges = &range;
                    params[paramCount++].ppBuffers = &uniformBuffer.pBuffer;

                    updateDescriptorSet(frame.m_renderer->Rend(), m_lightObjectIndex, m_lightPerLightSet, paramCount, params);
                    cmdBindDescriptorSet(frame.m_cmd, m_lightObjectIndex, m_lightPerLightSet);
                    m_lightObjectIndex = (m_lightObjectIndex + 1) % MaxLightUniforms;
                };

                for (auto& light : deferredLightStencilFrontRenderBack) {
                    std::array targets = { eVertexBufferElement_Position };
                    LegacyVertexBuffer::GeometryBinding binding{};
                    static_cast<LegacyVertexBuffer*>(GetLightShape(light->m_light, eDeferredShapeQuality_High))
                        ->resolveGeometryBinding(frame.m_currentFrame, targets, &binding);
                    detail::cmdDefaultLegacyGeomBinding(frame, binding);
                    cmdBindPipeline(frame.m_cmd, m_lightStencilPipeline);
                    cmdBindLightDescriptor(light);
                    cmdDrawIndexed(frame.m_cmd, binding.m_indexBuffer.numIndicies, 0, 0);

                    switch (light->m_light->GetLightType()) {
                        case eLightType_Point:
                            cmdBindPipeline(frame.m_cmd, m_pointLightPipeline);
                            break;
                        case eLightType_Spot:
                            cmdBindPipeline(frame.m_cmd, m_pointLightPipeline);
                            break;
                        default:
                            ASSERT(false && "Unsupported light type");
                            break;
                    }
                    cmdDrawIndexed(frame.m_cmd, binding.m_indexBuffer.numIndicies, 0, 0);

                }

                for (auto& light : deferredLightRenderBack) {
                    switch (light->m_light->GetLightType()) {
                        case eLightType_Point:
                            cmdBindPipeline(frame.m_cmd, m_pointLightPipeline);
                            break;
                        case eLightType_Spot:
                            cmdBindPipeline(frame.m_cmd, m_pointLightPipeline);
                            break;
                        default:
                            ASSERT(false && "Unsupported light type");
                            break;
                    }
                
                    GPURingBufferOffset uniformBuffer = getGPURingBufferOffset(m_lightPassRingBuffer, sizeof(LightUniformData));
                    std::array targets = { eVertexBufferElement_Position };
                    LegacyVertexBuffer::GeometryBinding binding{};
                    static_cast<LegacyVertexBuffer*>(GetLightShape(light->m_light, eDeferredShapeQuality_High))
                        ->resolveGeometryBinding(frame.m_currentFrame, targets, &binding);
                    cmdBindLightDescriptor(light);
                    detail::cmdDefaultLegacyGeomBinding(frame, binding);
                    cmdDrawIndexed(frame.m_cmd, binding.m_indexBuffer.numIndicies, 0, 0);
                }
                cmdEndDebugMarker(frame.m_cmd);

                {
                    cmdBindRenderTargets(frame.m_cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
                    std::array rtBarriers = {
                        RenderTargetBarrier{
                            currentGBuffer.m_outputBuffer.m_handle, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
                    };
                    cmdResourceBarrier(frame.m_cmd, 0, NULL, 0, NULL, rtBarriers.size(), rtBarriers.data());
                }
            }
        }
        // // ------------------------------------------------------------------------
        // // Render Light Pass --> renders to output target
        // // ------------------------------------------------------------------------
        // {
        //     LightPassOptions options = {
        //         .frustumProjection = mainFrustumProj,
        //         .frustumInvView = mainFrustumViewInv,
        //         .frustumView = mainFrustumView,
        //         .m_gBuffer = sharedData.m_gBuffer,
        //     };
        //     RenderLightPass(context, mpCurrentRenderList->GetLights(), apWorld, viewport, apFrustum, options);
        // }

        // // ------------------------------------------------------------------------
        // // Render Illumination Pass --> renders to output target
        // // ------------------------------------------------------------------------
        // {
        //     detail::IlluminationOptions args = {
        //         .projectionFrustum = mainFrustumProj,
        //         .viewFrustum = mainFrustumView,
        //         .buffer = sharedData.m_gBuffer,
        //         .name = "Illumination",
        //     };
        //     detail::RenderIlluminationPass(
        //         context, this, mpCurrentRenderList->GetRenderableItems(eRenderListType_Illumination), viewport, args);
        // }

        // // ------------------------------------------------------------------------
        // // Render Fog Pass --> output target
        // // ------------------------------------------------------------------------
        // auto fogRenderData = detail::createFogRenderData(mpCurrentRenderList->GetFogAreas(), apFrustum);
        // {
        //     FogPassOptions options = {
        //         .frustumProjection = mainFrustumProj,
        //         .frustumInvView = mainFrustumViewInv,
        //         .frustumView = mainFrustumView,
        //         .m_gBuffer = sharedData.m_gBuffer
        //     };
        //     RenderFogPass(context, fogRenderData, apWorld, viewport, apFrustum, options);
        // }

        // // ------------------------------------------------------------------------
        // // Render Fullscreen Fog Pass --> output target
        // // ------------------------------------------------------------------------
        
        // if (mpCurrentWorld->GetFogActive()) {
        //     FogPassFullscreenOptions options = {
        //         .m_gBuffer = sharedData.m_gBuffer
        //     };
        //     RenderFullscreenFogPass(context, apWorld, viewport, apFrustum, options);
        // }

        // // notify post draw listeners
        // ImmediateDrawBatch postSolidBatch(context, sharedData.m_gBuffer.m_outputTarget, mainFrustumView, mainFrustumProj);
        // cViewport::PostSolidDrawPacket postSolidEvent = cViewport::PostSolidDrawPacket({
        //     .m_frustum = apFrustum,
        //     .m_context = &context,
        //     .m_outputTarget = &sharedData.m_gBuffer.m_outputTarget,
        //     .m_viewport = &viewport,
        //     .m_renderSettings = mpCurrentSettings,
        //     .m_immediateDrawBatch = &postSolidBatch,
        // });
        // viewport.SignalDraw(postSolidEvent);
        // postSolidBatch.flush();

        // ([&]() {
        //     auto translucentSpan = mpCurrentRenderList->GetRenderableItems(eRenderListType_Translucent);
        //     if (translucentSpan.empty()) {
        //         return;
        //     }

        //     GraphicsContext::ViewConfiguration viewConfig{ sharedData.m_gBuffer.m_outputTarget };
        //     viewConfig.m_projection = mainFrustumProj;
        //     viewConfig.m_view = mainFrustumView;
        //     viewConfig.m_viewRect = { 0, 0, sharedData.m_size.x, sharedData.m_size.y };
        //     auto view = context.StartPass("Translucent", viewConfig);
        //     bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
        //     const float fHalfFovTan = tan(apFrustum->GetFOV() * 0.5f);
        //     for (auto& obj : translucentSpan) {
        //         auto* pMaterial = obj->GetMaterial();
        //         auto* pMaterialType = pMaterial->GetType();
        //         auto* vertexBuffer = obj->GetVertexBuffer();

        //         cMatrixf* pMatrix = obj->GetModelMatrix(apFrustum);

        //         eMaterialRenderMode renderMode =
        //             mpCurrentWorld->GetFogActive() ? eMaterialRenderMode_DiffuseFog : eMaterialRenderMode_Diffuse;
        //         if (!pMaterial->GetAffectedByFog()) {
        //             renderMode = eMaterialRenderMode_Diffuse;
        //         }

        //         ////////////////////////////////////////
        //         // Check the fog area alpha
        //         mfTempAlpha = 1;
        //         if (pMaterial->GetAffectedByFog()) {
        //             for (auto& fogArea : fogRenderData) {
        //                 mfTempAlpha *= detail::GetFogAreaVisibilityForObject(fogArea, *apFrustum, obj);
        //             }
        //         }

        //         if (!obj->UpdateGraphicsForViewport(apFrustum, mfCurrentFrameTime)) {
        //             continue;
        //         }

        //         if (!obj->RetrieveOcculsionQuery(this)) {
        //             continue;
        //         }
        //         // if (!CheckRenderablePlaneIsVisible(obj, mpCurrentFrustum)) {
        //         //     continue;
        //         // }

        //         if (pMaterial->HasRefraction()) {
        //             cBoundingVolume* pBV = obj->GetBoundingVolume();
        //             cRect2l clipRect = rendering::detail::GetClipRectFromObject(obj, 0.2f, apFrustum, sharedData.m_size, fHalfFovTan);
        //             if (clipRect.w >= 0 || clipRect.h >= 0) {
        //                 GraphicsContext::ShaderProgram shaderInput;
        //                 shaderInput.m_handle = m_copyRegionProgram;
        //                 shaderInput.m_textures.push_back({ m_s_diffuseMap, sharedData.m_gBuffer.m_outputImage->GetHandle(), 0 });
        //                 shaderInput.m_uavImage.push_back(
        //                     { sharedData.m_refractionImage->GetHandle(),1, 0, bgfx::Access::Write, bgfx::TextureFormat::Enum::RGBA8 });

        //                 float copyRegion[4] = { static_cast<float>(clipRect.x),
        //                                         static_cast<float>(sharedData.m_size.y - (clipRect.h + clipRect.y)),
        //                                         static_cast<float>(clipRect.w),
        //                                         static_cast<float>(clipRect.h) };
        //                 shaderInput.m_uniforms.push_back({ m_u_copyRegion, &copyRegion, 1 });

        //                 GraphicsContext::ComputeRequest computeRequest{
        //                     shaderInput, static_cast<uint32_t>((clipRect.w / 16) + 1), static_cast<uint32_t>((clipRect.h / 16) + 1), 1
        //                 };
        //                 context.Submit(view, computeRequest);
        //             }
        //         }

        //         if (pMaterial->HasWorldReflection() && obj->GetRenderType() == eRenderableType_SubMesh) {
        //             auto* reflectionSubMeshEntity = static_cast<cSubMeshEntity*>(obj);
        //             bool bReflectionIsInRange = true;
        //             if (pMaterial->GetMaxReflectionDistance() > 0) {
        //                 cVector3f point = apFrustum->GetOrigin() + apFrustum->GetForward() * -1 * pMaterial->GetMaxReflectionDistance();
        //                 cVector3f normal = apFrustum->GetForward();
        //                 cPlanef maxRelfctionDistPlane;
        //                 maxRelfctionDistPlane.FromNormalPoint(normal, point);
        //                 if (cMath::CheckPlaneBVCollision(maxRelfctionDistPlane, *reflectionSubMeshEntity->GetBoundingVolume()) == eCollision_Outside) {
        //                     bReflectionIsInRange = false;
        //                 }
        //             }

        //             if(mpCurrentSettings->mbRenderWorldReflection && bReflectionIsInRange && reflectionSubMeshEntity->GetIsOneSided() && false)
		//             {
        //                 cSubMesh *pSubMesh = reflectionSubMeshEntity->GetSubMesh();
        //                 cVector3f reflectionSurfaceNormal = cMath::Vector3Normalize(cMath::MatrixMul3x3(reflectionSubMeshEntity->GetWorldMatrix(), pSubMesh->GetOneSidedNormal()));
        //                 cVector3f reflectionSurfacePosition = cMath::MatrixMul(reflectionSubMeshEntity->GetWorldMatrix(), pSubMesh->GetOneSidedPoint());

        //                 cPlanef reflectPlane;
        //                 reflectPlane.FromNormalPoint(reflectionSurfaceNormal, reflectionSurfacePosition);

        //                 cMatrixf reflectionMatrix = cMath::MatrixPlaneMirror(reflectPlane);
        //                 cMatrixf reflectionView = cMath::MatrixMul(apFrustum->GetViewMatrix(), reflectionMatrix);
        //                 cVector3f reflectionOrigin = cMath::MatrixMul(reflectionMatrix, apFrustum->GetOrigin());

        //                 cMatrixf reflectionProjectionMatrix = apFrustum->GetProjectionMatrix();

        //                 cPlanef cameraSpaceReflPlane = cMath::TransformPlane(reflectionView, reflectPlane);
        //                 cMatrixf mtxReflProj = cMath::ProjectionMatrixObliqueNearClipPlane(reflectionProjectionMatrix, cameraSpaceReflPlane);

        //                 cFrustum reflectFrustum;
        //                 reflectFrustum.SetupPerspectiveProj(mtxReflProj, reflectionView,
        //                     apFrustum->GetFarPlane(),apFrustum->GetNearPlane(),
        //                     apFrustum->GetFOV(), apFrustum->GetAspect(),
        //                     reflectionOrigin,false, &reflectionProjectionMatrix, true);
        //                 reflectFrustum.SetInvertsCullMode(true);
                        
        //                 std::vector<cPlanef> m_occlusionPlanes;
                        
        //                 const float fMaxReflDist = pMaterial->GetMaxReflectionDistance();
        //                 if(pMaterial->GetMaxReflectionDistance() > 0) { 
        //                     //Forward and normal is aligned, the normal of plane becomes inverse forward
        //                     cVector3f frustumForward = apFrustum->GetForward()*-1;
		// 	                float fFDotN = cMath::Vector3Dot(frustumForward, reflectionSurfaceNormal);
        //                     cPlanef maxRelfctionDistPlane;
        //                     if(fFDotN <-0.99999f) {
        //                         cVector3f vClipNormal, vClipPoint;
        //                         vClipNormal = frustumForward*-1;
        //                         vClipPoint = apFrustum->GetOrigin() + frustumForward * pMaterial->GetMaxReflectionDistance();
        //                         maxRelfctionDistPlane.FromNormalPoint(vClipNormal, vClipPoint);
        //                     } else {
        //                         cPlanef cameraSpacePlane = cMath::TransformPlane(apFrustum->GetViewMatrix(), reflectPlane);

        //                         cVector3f vPoint1 = cVector3f(0,0, -fMaxReflDist);
        //                         cVector3f vPoint2 = cVector3f(0,0, -fMaxReflDist);

        //                         //Vertical row (x always same)
        //                         if(fabs(cameraSpacePlane.b) < 0.0001f)
        //                         {
        //                             vPoint1.x = (-cameraSpacePlane.c*-fMaxReflDist - cameraSpacePlane.d) / cameraSpacePlane.a;
        //                             vPoint2 = vPoint1;
        //                             vPoint2.y+=1;
        //                         }
        //                         //Horizontal row (y always same)
        //                         else if(fabs(cameraSpacePlane.a) < 0.0001f)
        //                         {
        //                             vPoint1.y = (-cameraSpacePlane.c*-fMaxReflDist - cameraSpacePlane.d) / cameraSpacePlane.b;
        //                             vPoint2 = vPoint1;
        //                             vPoint2.x+=1;
        //                         }
        //                         //Oblique row (x and y changes)
        //                         else
        //                         {
        //                             vPoint1.x = (-cameraSpacePlane.c*-fMaxReflDist - cameraSpacePlane.d) / cameraSpacePlane.a;
        //                             vPoint2.y = (-cameraSpacePlane.c*-fMaxReflDist - cameraSpacePlane.d) / cameraSpacePlane.b;
        //                         }

        //                         cMatrixf mtxInvCamera = cMath::MatrixInverse(apFrustum->GetViewMatrix());
        //                         vPoint1 = cMath::MatrixMul(mtxInvCamera, vPoint1);
        //                         vPoint2 = cMath::MatrixMul(mtxInvCamera, vPoint2);

        //                         cVector3f vNormal = cMath::Vector3Cross(vPoint1-reflectionOrigin, vPoint2-reflectionOrigin);
        //                         vNormal.Normalize();
        //                         //make sure normal has correct sign!
        //                         if(cMath::Vector3Dot(reflectionSurfaceNormal, vNormal)<0) {
        //                             vNormal = vNormal*-1;
        //                         }
                                
        //                         maxRelfctionDistPlane.FromNormalPoint(vNormal, vPoint1);
        //                     }
        //                     m_occlusionPlanes.push_back(maxRelfctionDistPlane);
        //                 }
        //                 const cMatrixf reflectionFrustumView = reflectFrustum.GetViewMatrix().GetTranspose();
        //                 const cMatrixf reflectionFrustumProj = reflectFrustum.GetProjectionMatrix().GetTranspose();
        //                 const cMatrixf reflectionFrustumViewInv = cMath::MatrixInverse(reflectFrustum.GetViewMatrix());
        
        //                 auto createReflectionViewConfig = [&](LegacyRenderTarget& rt) -> GraphicsContext::ViewConfiguration {
        //                     GraphicsContext::ViewConfiguration viewConfig{ rt };
        //                     viewConfig.m_projection = reflectionFrustumProj;
        //                     viewConfig.m_view = reflectionFrustumView;
        //                     viewConfig.m_viewRect = { 0, 0, sharedData.m_size.x, sharedData.m_size.y };
        //                     return viewConfig;
        //                 };

        //                 [&] {
        //                     GraphicsContext::ViewConfiguration viewConfig{ sharedData.m_gBufferReflection.m_fullTarget };
        //                     viewConfig.m_viewRect = { 0, 0, sharedData.m_size.x, sharedData.m_size.y };
        //                     viewConfig.m_clear = { 0, 1, 0, ClearOp::Depth | ClearOp::Stencil | ClearOp::Color };
        //                     bgfx::touch(context.StartPass("Clear Depth", viewConfig));
        //                 }();
        //                 auto ZPassConfig = createReflectionViewConfig(sharedData.m_gBufferReflection.m_depthTarget);
        //                 m_reflectionRenderList.Setup(mfCurrentFrameTime, &reflectFrustum);
        //                 detail::UpdateRenderableList(
        //                     &m_reflectionRenderList, 
        //                     this, 
        //                     mpCurrentSettings->mpVisibleNodeTracker,
        //                     &reflectFrustum, mpCurrentWorld,"Z Reflection Pass", ZPassConfig, context, viewport, 
        //                     eObjectVariabilityFlag_All, 
        //                     mvCurrentOcclusionPlanes, 
        //                     eRenderableFlag_VisibleInReflection);
        //                 m_reflectionRenderList.Compile( eRenderListCompileFlag_Diffuse | 
        //                     eRenderListCompileFlag_Translucent | 
        //                     eRenderListCompileFlag_Decal |
        //                     eRenderListCompileFlag_Illumination | 
        //                     eRenderListCompileFlag_FogArea);
        //                 auto viewGbufferCfg = createReflectionViewConfig(sharedData.m_gBufferReflection.m_fullTarget);
        //                 // detail::RenderGBufferPass(
        //                 //     "Reflection_GBuffer", 
        //                 //     context, 
        //                 //     this, 
        //                 //     viewGbufferCfg, 
        //                 //     m_reflectionRenderList.GetRenderableItems(eRenderListType_Diffuse), viewport);

        //                 // ------------------------------------------------------------------------------------
        //                 //  Render Decal Pass render to color and depth
        //                 // ------------------------------------------------------------------------------------
        //                 // auto viewDecalCfg = createReflectionViewConfig(sharedData.m_gBufferReflection_colorAndDepth);
        //                 // detail::RenderDecalPass(
        //                 //     "Reflection Decal", 
        //                 //     context, 
        //                 //     this, 
        //                 //     viewDecalCfg, 
        //                 //     m_reflectionRenderList.GetRenderableItems(eRenderListType_Decal), viewport);
                        
        //                 // ------------------------------------------------------------------------
        //                 // Render Light Pass --> renders to output target
        //                 // ------------------------------------------------------------------------
        //                 // {
        //                 //     LightPassOptions options = {
        //                 //         .frustumProjection = reflectionFrustumProj,
        //                 //         .frustumInvView = reflectionFrustumViewInv,
        //                 //         .frustumView = reflectionFrustumView,
        //                 //         .m_output_target = sharedData.m_outputReflection_target,

        //                 //         .m_gBufferColor = *sharedData.m_gBufferReflectionColor,
        //                 //         .m_gBufferNormalImage = *sharedData.m_gBufferReflectionNormalImage,
        //                 //         .m_gBufferPositionImage = *sharedData.m_gBufferReflectionPositionImage,
        //                 //         .m_gBufferSpecular = *sharedData.m_gBufferReflectionSpecular,
        //                 //         .m_gBufferDepthStencil = *sharedData.m_gBufferReflectionDepthStencil,
        //                 //         .m_outputImage = *sharedData.m_outputReflectionImage,
        //                 //     };
        //                 //     RenderLightPass(context, m_reflectionRenderList.GetLights(), apWorld, viewport, &reflectFrustum, options);
        //                 // }
        //                 // ------------------------------------------------------------------------
        //                 // Render Illumination Pass --> renders to output target
        //                 // ------------------------------------------------------------------------
        //                 // auto illuminationCfg = createStandardViewConfig(sharedData.m_gBufferReflection_colorAndDepth);
        //                 // detail::RenderIlluminationPass(
        //                 //     "RenderIllumination", 
        //                 //     context, 
        //                 //     this, 
        //                 //     illuminationCfg, 
        //                 //     m_reflectionRenderList.GetRenderableItems(eRenderListType_Illumination), viewport);

        //                 // auto reflectionfogRenderData = detail::createFogRenderData(m_reflectionRenderList.GetFogAreas(), &reflectFrustum);
        //                 // {
        //                 //     FogPassOptions options = {
        //                 //         .frustumProjection = reflectionFrustumProj,
        //                 //         .frustumInvView = reflectionFrustumViewInv,
        //                 //         .frustumView = reflectionFrustumView,
        //                 //         .m_output_target = sharedData.m_outputReflection_target,
        //                 //         .m_gBufferPositionImage = *sharedData.m_gBufferReflectionPositionImage,
        //                 //     };
        //                 //     RenderFogPass(context, reflectionfogRenderData, apWorld, viewport, &reflectFrustum, options);
        //                 // }

        //             }
        //         }

        //         pMaterialType->ResolveShaderProgram(
        //             renderMode, viewport, pMaterial, obj, this, [&](GraphicsContext::ShaderProgram& shaderInput) {
        //                 GraphicsContext::LayoutStream layoutInput;
        //                 vertexBuffer->GetLayoutStream(layoutInput);

        //                 shaderInput.m_configuration.m_depthTest = pMaterial->GetDepthTest() ? DepthTest::LessEqual : DepthTest::None;
        //                 shaderInput.m_configuration.m_write = Write::RGB;
        //                 shaderInput.m_configuration.m_cull = Cull::CounterClockwise;
    
        //                 shaderInput.m_modelTransform = pMatrix ? pMatrix->GetTranspose() : cMatrixf::Identity;

        //                 if (pMaterial->HasRefraction()) {
        //                     shaderInput.m_configuration.m_rgbBlendFunc = CreateBlendFunction(BlendOperator::Add, BlendOperand::SrcAlpha, BlendOperand::One);
        //                     shaderInput.m_configuration.m_alphaBlendFunc = CreateFromMaterialBlendMode(eMaterialBlendMode_Add);
        //                 } else {
        //                     shaderInput.m_configuration.m_rgbBlendFunc = CreateFromMaterialBlendMode(pMaterial->GetBlendMode());
        //                     shaderInput.m_configuration.m_alphaBlendFunc = CreateFromMaterialBlendMode(pMaterial->GetBlendMode());
        //                 }

        //                 GraphicsContext::DrawRequest drawRequest{ layoutInput, shaderInput };
        //                 context.Submit(view, drawRequest);
        //             });

        //         if (pMaterial->HasTranslucentIllumination()) {
        //             pMaterialType->ResolveShaderProgram(
        //                 (renderMode == eMaterialRenderMode_Diffuse ? eMaterialRenderMode_Illumination
        //                                                            : eMaterialRenderMode_IlluminationFog),
        //                 viewport,
        //                 pMaterial,
        //                 obj,
        //                 this,
        //                 [&](GraphicsContext::ShaderProgram& shaderInput) {
        //                     GraphicsContext::LayoutStream layoutInput;
        //                     vertexBuffer->GetLayoutStream(layoutInput);

        //                     shaderInput.m_configuration.m_depthTest = pMaterial->GetDepthTest() ? DepthTest::LessEqual : DepthTest::None;
        //                     shaderInput.m_configuration.m_write = Write::RGB;
        //                     shaderInput.m_configuration.m_cull = Cull::CounterClockwise;

        //                     shaderInput.m_configuration.m_rgbBlendFunc = CreateFromMaterialBlendMode(eMaterialBlendMode_Add);
        //                     shaderInput.m_configuration.m_alphaBlendFunc = CreateFromMaterialBlendMode(eMaterialBlendMode_Add);

        //                     shaderInput.m_modelTransform = pMatrix ? pMatrix->GetTranspose() : cMatrixf::Identity;

        //                     GraphicsContext::DrawRequest drawRequest{ layoutInput, shaderInput };
        //                     context.Submit(view, drawRequest);
        //                 });
        //         }
        //     }
        // })();

        // ImmediateDrawBatch postTransBatch(context, sharedData.m_gBuffer.m_outputTarget, mainFrustumView, mainFrustumProj);
        // cViewport::PostTranslucenceDrawPacket translucenceEvent = cViewport::PostTranslucenceDrawPacket({
        //     .m_frustum = apFrustum,
        //     .m_context = &context,
        //     .m_outputTarget = &sharedData.m_gBuffer.m_outputTarget,
        //     .m_viewport = &viewport,
        //     .m_renderSettings = mpCurrentSettings,
        //     .m_immediateDrawBatch = &postTransBatch,
        // });
        // viewport.SignalDraw(translucenceEvent);
        // postTransBatch.flush();
    }
    
    iVertexBuffer* cRendererDeferred::GetLightShape(iLight* apLight, eDeferredShapeQuality aQuality) const {
        switch(apLight->GetLightType()) {
            case eLightType_Point:
                return m_shapeSphere[aQuality].get();
            case eLightType_Spot:
                return m_shapePyramid.get();
            default:
                break;
        }
        return nullptr;
    }

} // namespace hpl
