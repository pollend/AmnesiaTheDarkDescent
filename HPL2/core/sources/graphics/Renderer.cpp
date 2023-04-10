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

#include "graphics/Renderer.h"

#include "bgfx/bgfx.h"
#include "graphics/Enum.h"
#include "graphics/GraphicsContext.h"
#include "graphics/RenderTarget.h"
#include "graphics/ShaderUtil.h"
#include "math/BoundingVolume.h"
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
#include "graphics/MeshCreator.h"
#include "graphics/RenderList.h"
#include "graphics/Renderable.h"
#include "graphics/SubMesh.h"
#include "graphics/Texture.h"
#include "graphics/VertexBuffer.h"

#include "resources/LowLevelResources.h"
#include "resources/MeshManager.h"
#include "resources/Resources.h"
#include "resources/TextureManager.h"

#include "scene/Camera.h"
#include "scene/FogArea.h"
#include "scene/LightBox.h"
#include "scene/LightPoint.h"
#include "scene/LightSpot.h"
#include "scene/RenderableContainer.h"
#include "scene/SubMeshEntity.h"
#include "scene/World.h"

#include <algorithm>
#include <memory>

namespace hpl
{

    eShadowMapQuality iRenderer::mShadowMapQuality = eShadowMapQuality_Medium;
    eShadowMapResolution iRenderer::mShadowMapResolution = eShadowMapResolution_High;
    eParallaxQuality iRenderer::mParallaxQuality = eParallaxQuality_Low;
    bool iRenderer::mbParallaxEnabled = true;
    int iRenderer::mlReflectionSizeDiv = 2;
    bool iRenderer::mbRefractionEnabled = true;

    //-----------------------------------------------------------------------

    int iRenderer::mlRenderFrameCount = 0;
        
    namespace rendering::detail
    {
        eShadowMapResolution GetShadowMapResolution(eShadowMapResolution aWanted, eShadowMapResolution aMax)
        {
            if (aMax == eShadowMapResolution_High)
            {
                return aWanted;
            }
            else if (aMax == eShadowMapResolution_Medium)
            {
                return aWanted == eShadowMapResolution_High ? eShadowMapResolution_Medium : aWanted;
            }
            return eShadowMapResolution_Low;
        }

        bool IsObjectVisible(iRenderable* apObject, tRenderableFlag alNeededFlags, std::span<cPlanef> occlusionPlanes)
        {
            if (!apObject->IsVisible())
            {
                return false;
            }

            if ((apObject->GetRenderFlags() & alNeededFlags) != alNeededFlags)
            {
                return false;
            }

            for (auto& plane : occlusionPlanes)
            {
                cBoundingVolume* pBV = apObject->GetBoundingVolume();
                if (cMath::CheckPlaneBVCollision(plane, *pBV) == eCollision_Outside)
                {
                    return false;
                }
            }
            return true;
        }

        void RenderableMaterialIter(
            iRenderer* renderer, 
            std::span<iRenderable*> iter,
            cViewport& viewport,
            eMaterialRenderMode mode,
            RenderableIterCallback handler) {
                for (auto& obj : iter)
                {
                    GraphicsContext::LayoutStream layoutStream;
                    GraphicsContext::ShaderProgram shaderProgram;

                    cMaterial* pMaterial = obj->GetMaterial();
                    iMaterialType* materialType = pMaterial->GetType();
                    iVertexBuffer* vertexBuffer = obj->GetVertexBuffer();
                    if (vertexBuffer == nullptr || materialType == nullptr)
                    {
                        continue;
                    }
                    vertexBuffer->GetLayoutStream(layoutStream);
                    materialType->ResolveShaderProgram(
                        mode,
                        viewport,
                        pMaterial,
                        obj,
                        renderer,
                        [&](GraphicsContext::ShaderProgram& program)
                        {
                            handler(obj, layoutStream, program);
                        });
                }
        }

        bool checkNodeIsVisible(iRenderableContainerNode* apNode, std::span<cPlanef> clipPlanes) {
            for(auto& plane: clipPlanes)
            {
                if (cMath::CheckPlaneAABBCollision(plane, apNode->GetMin(), apNode->GetMax(), apNode->GetCenter(), apNode->GetRadius()) ==
                    eCollision_Outside)
                {
                    return false;
                }
            }
            return true;
        }

        bool CheckIfObjectIsVisible(
            iRenderable* object, 
            tRenderableFlag neededFlags,
            std::span<cPlanef> clipPlanes) {

            if (object->IsVisible() == false)
                return false;

            /////////////////////////////
            // Check flags
            if ((object->GetRenderFlags() & neededFlags) != neededFlags)
                return false;

            if (!clipPlanes.empty())
            {
                cBoundingVolume* pBV = object->GetBoundingVolume();
                for(auto& planes: clipPlanes)
                {
                    if (cMath::CheckPlaneBVCollision(planes, *pBV) == eCollision_Outside) {
                        return false;
                    }
                }
            }
            return true;
        }
    } // namespace rendering::detail

    bool cRendererNodeSortFunc::operator()(const iRenderableContainerNode* apNodeA, const iRenderableContainerNode* apNodeB) const
    {
        if (apNodeA->IsInsideView() != apNodeB->IsInsideView())
        {
            return apNodeA->IsInsideView() > apNodeB->IsInsideView();
        }

        return apNodeA->GetViewDistance() > apNodeB->GetViewDistance();
    }

    cRenderSettings::cRenderSettings(bool abIsReflection)
    {
        ////////////////////////
        // Create data
        mpRenderList = hplNew(cRenderList, ());

        mpVisibleNodeTracker = hplNew(cVisibleRCNodeTracker, ());

        ////////////////////////
        // Setup data
        mlCurrentOcclusionObject = 0;

        ////////////////////////
        // Set up General Variables
        mbIsReflection = abIsReflection;
        mbLog = false;

        mClearColor = cColor(0, 0);

        ////////////////////////
        // Set up Render Variables
        // Change this later I assume:
        mlMinimumObjectsBeforeOcclusionTesting = 0; // 8;//8 should be good default, giving a good amount of colliders, or? Clarifiction:
                                                    // Minium num of object rendered until node visibility tests start!
        mlSampleVisiblilityLimit = 3;

        mbUseCallbacks = true;

        mbUseEdgeSmooth = false;

        mbUseOcclusionCulling = true;

        mMaxShadowMapResolution = eShadowMapResolution_High;
        if (mbIsReflection)
            mMaxShadowMapResolution = eShadowMapResolution_Medium;

        mbClipReflectionScreenRect = true;

        mbUseScissorRect = false;

        mbRenderWorldReflection = true;

        ////////////////////////
        // Set up Shadow Variables
        mbRenderShadows = true;
        mfShadowMapBias = 4; // The constant bias
        mfShadowMapSlopeScaleBias = 2; // The bias based on sloping of depth.

        ////////////////////////////
        // Light settings
        mbSSAOActive = mbIsReflection ? false : true;

        ////////////////////////
        // Set up Output Variables
        mlNumberOfLightsRendered = 0;
        mlNumberOfOcclusionQueries = 0;

        ////////////////////////
        // Set up Private Variables

        ////////////////////////
        // Create Reflection settings
        mpReflectionSettings = NULL;
        if (mbIsReflection == false)
        {
            mpReflectionSettings = hplNew(cRenderSettings, (true));
        }
    }

    cRenderSettings::~cRenderSettings()
    {
        hplDelete(mpRenderList);
        hplDelete(mpVisibleNodeTracker);

        STLDeleteAll(mvOcclusionObjectPool);

        if (mpReflectionSettings)
            hplDelete(mpReflectionSettings);
    }

    //-----------------------------------------------------------------------

    void cRenderSettings::ResetVariables()
    {
        // return;
        mpVisibleNodeTracker->Reset();

        if (mpReflectionSettings)
            mpReflectionSettings->ResetVariables();
    }

//-----------------------------------------------------------------------

//////////////////////////////////////////////////
// The render settings will use the default setup, except for the variables below
//  This means SSAO, edgesmooth, etc are always off for reflections.
#define RenderSettingsCopy(aVar) mpReflectionSettings->aVar = aVar
    void cRenderSettings::SetupReflectionSettings()
    {
        if (mpReflectionSettings == NULL)
            return;
        RenderSettingsCopy(mbLog);

        RenderSettingsCopy(mClearColor);

        ////////////////////////////
        // Render settings
        RenderSettingsCopy(mlMinimumObjectsBeforeOcclusionTesting);
        RenderSettingsCopy(mlSampleVisiblilityLimit);

        mpReflectionSettings->mbUseScissorRect = false;

        ////////////////////////////
        // Shadow settings
        RenderSettingsCopy(mbRenderShadows);
        RenderSettingsCopy(mfShadowMapBias);
        RenderSettingsCopy(mfShadowMapSlopeScaleBias);

        ////////////////////////////
        // Light settings
        // RenderSettingsCopy(mbSSAOActive);

        ////////////////////////////
        // Output
        RenderSettingsCopy(mlNumberOfLightsRendered);
        RenderSettingsCopy(mlNumberOfOcclusionQueries);
    }


    void cRenderSettings::AssignOcclusionObject(
        iRenderer* apRenderer, void* apSource, int alCustomIndex, iVertexBuffer* apVtxBuffer, cMatrixf* apMatrix, bool abDepthTest)
    {
        if (mlCurrentOcclusionObject == mvOcclusionObjectPool.size())
        {
            mvOcclusionObjectPool.push_back(hplNew(cOcclusionQueryObject, ()));
        }

        cOcclusionQueryObject* pObject = mvOcclusionObjectPool[mlCurrentOcclusionObject];

        pObject->mlCustomID = alCustomIndex;
        pObject->m_occlusion = bgfx::createOcclusionQuery();
        pObject->mpVtxBuffer = apVtxBuffer;
        pObject->mpMatrix = apMatrix;
        pObject->mbDepthTest = abDepthTest;

        m_setOcclusionObjects.insert(tOcclusionQueryObjectMap::value_type(apSource, pObject));

        ++mlCurrentOcclusionObject;
    }

    //-----------------------------------------------------------------------

    int cRenderSettings::RetrieveOcclusionObjectSamples(iRenderer* apRenderer, void* apSource, int alCustomIndex)
    {
        tOcclusionQueryObjectMapIt it = m_setOcclusionObjects.find(apSource);
        if (it == m_setOcclusionObjects.end())
        {
            if (mbLog)
                Log(" Could not find source %d custom index %d in occlusion objects set!\n", apSource, alCustomIndex);
            return 0;
        }

        //////////////////////////////////////
        // Get the number of objects with key and get the one with right custom ID
        size_t lCount = m_setOcclusionObjects.count(apSource);
        cOcclusionQueryObject* pObject = NULL;
        for (size_t i = 0; i < lCount; ++i)
        {
            cOcclusionQueryObject* pTestObject = it->second;
            if (pTestObject->mlCustomID == alCustomIndex)
            {
                pObject = pTestObject;
                break;
            }
            it++;
        }
        if (pObject == NULL)
        {
            if (mbLog)
                Log(" Found source %d but could NOT find custom index %d in occlusion objects set!\n", apSource, alCustomIndex);
            return 0;
        }

        if (bgfx::isValid(pObject->m_occlusion))
        {
            int32_t numSamples = 0;
            if (bgfx::getResult(pObject->m_occlusion, &numSamples) == bgfx::OcclusionQueryResult::Visible)
            {
                pObject->mlSampleResults = numSamples;
                bgfx::destroy(pObject->m_occlusion);
                pObject->m_occlusion = BGFX_INVALID_HANDLE;
            }
        }
        return 0;
    }

    //-----------------------------------------------------------------------

    void cRenderSettings::WaitAndRetrieveAllOcclusionQueries(iRenderer* apRenderer)
    {
        // for(int i=0; i<mlCurrentOcclusionObject; ++i)
        // {
        // 	iOcclusionQuery *pQuery = mvOcclusionObjectPool[i]->mpQuery;
        // 	if(pQuery==NULL) continue;

        //     while(pQuery->FetchResults()==false);

        // 	mvOcclusionObjectPool[i]->mlSampleResults = pQuery->GetSampleCount();
        // 	mvOcclusionObjectPool[i]->mpQuery = NULL;
        // 	apRenderer->ReleaseOcclusionQuery(pQuery);
        // }
    }

    //-----------------------------------------------------------------------

    void cRenderSettings::ClearOcclusionObjects(iRenderer* apRenderer)
    {
        // if(mbLog) Log(" Clearing occlusion queries i settings!\n");
        // m_setOcclusionObjects.clear();
        // for(int i=0; i<mlCurrentOcclusionObject; ++i)
        // {
        // 	iOcclusionQuery *pQuery = mvOcclusionObjectPool[i]->mpQuery;
        // 	if(pQuery==NULL) continue;

        // 	apRenderer->ReleaseOcclusionQuery(pQuery);
        // 	mvOcclusionObjectPool[i]->mpQuery = NULL;
        // }

        // mlCurrentOcclusionObject = 0;
    }

    //-----------------------------------------------------------------------

    //////////////////////////////////////////////////////////////////////////
    // SHADOWMAP CACHE
    //////////////////////////////////////////////////////////////////////////

    //-----------------------------------------------------------------------

    void cShadowMapLightCache::SetFromLight(iLight* apLight)
    {
        mpLight = apLight;
        mlTransformCount = apLight->GetTransformUpdateCount();
        mfRadius = apLight->GetRadius();

        if (apLight->GetLightType() == eLightType_Spot)
        {
            cLightSpot* pSpotLight = static_cast<cLightSpot*>(apLight);
            mfAspect = pSpotLight->GetAspect();
            mfFOV = pSpotLight->GetFOV();
        }
    }

    //-----------------------------------------------------------------------

    //////////////////////////////////////////////////////////////////////////
    // CONSTRUCTORS
    //////////////////////////////////////////////////////////////////////////

    //-----------------------------------------------------------------------

    iRenderer::iRenderer(const tString& asName, cGraphics* apGraphics, cResources* apResources, int alNumOfProgramComboModes)
    {
        mpGraphics = apGraphics;
        mpResources = apResources;

        //////////////
        // Set variables from arguments
        msName = asName;

        /////////////////////////////////
        // Set up the render functions
        SetupRenderFunctions(mpGraphics->GetLowLevel());

        ////////////////////////
        // Debug Variables
        mbOnlyRenderPrevVisibleOcclusionObjects = false;
        mlOnlyRenderPrevVisibleOcclusionObjectsFrameCount = 0;

        //////////////
        // Init variables
        mfDefaultAlphaLimit = 0.5f;

        mbSetFrameBufferAtBeginRendering = true;
        mbClearFrameBufferAtBeginRendering = true;
        mbSetupOcclusionPlaneForFog = false;

        mfScissorLastFov = 0;
        mfScissorLastTanHalfFov = 0;

        // mpCallbackFunctions = hplNew( cRendererCallbackFunctions, (this) );

        mfTimeCount = 0;

        mlActiveOcclusionQueryNum = 0;

        m_nullShader = hpl::loadProgram("vs_null", "fs_null");

        ////////////
        // Create shapes
        //  Color and Texture because Geforce cards fail without it, no idea why...
        auto loadVertexBufferFromMesh = [&](const tString& meshName, tVertexElementFlag vtxFlag) {
			iVertexBuffer* pVtxBuffer = mpResources->GetMeshManager()->CreateVertexBufferFromMesh(meshName, vtxFlag);
			if (pVtxBuffer == NULL) {
				FatalError("Could not load vertex buffer from mesh '%s'\n", meshName.c_str());
			}
			return pVtxBuffer;
		};
        mpShapeBox = loadVertexBufferFromMesh("core_box.dae", eVertexElementFlag_Position | eVertexElementFlag_Texture0 | eVertexElementFlag_Color0);
    }

    //-----------------------------------------------------------------------

    iRenderer::~iRenderer()
    {
        if (bgfx::isValid(m_nullShader))
        {
            bgfx::destroy(m_nullShader);
        }

        
        if (mpShapeBox)
            hplDelete(mpShapeBox);

        // hplDelete(mpCallbackFunctions);
    }

    //-----------------------------------------------------------------------

    //////////////////////////////////////////////////////////////////////////
    // PUBLIC METHODS
    //////////////////////////////////////////////////////////////////////////

    //-----------------------------------------------------------------------

    void iRenderer::Render(
        float afFrameTime,
        cFrustum* apFrustum,
        cWorld* apWorld,
        cRenderSettings* apSettings,
        bool abSendFrameBufferToPostEffects)
    {
        BeginRendering(afFrameTime, apFrustum, apWorld, apSettings, abSendFrameBufferToPostEffects);
    }

    // void iRenderer::RenderableHelper(
    //     eRenderListType type,
    //     cViewport& viewport,
    //     eMaterialRenderMode mode,
    //     std::function<void(iRenderable* obj, GraphicsContext::LayoutStream&, GraphicsContext::ShaderProgram&)> handler)
    // {
    //     for (auto& obj : mpCurrentRenderList->GetRenderableItems(type))
    //     {
    //         GraphicsContext::LayoutStream layoutStream;
    //         GraphicsContext::ShaderProgram shaderProgram;

    //         cMaterial* pMaterial = obj->GetMaterial();
    //         iMaterialType* materialType = pMaterial->GetType();
    //         // iGpuProgram* program = pMaterial->GetProgram(0, mode);
    //         iVertexBuffer* vertexBuffer = obj->GetVertexBuffer();
    //         if (vertexBuffer == nullptr || materialType == nullptr)
    //         {
    //             continue;
    //         }
    //         vertexBuffer->GetLayoutStream(layoutStream);
    //         materialType->ResolveShaderProgram(
    //             mode,
    //             viewport,
    //             pMaterial,
    //             obj,
    //             this,
    //             [&](GraphicsContext::ShaderProgram& program, uint32_t variant)
    //             {
    //                 handler(obj, layoutStream, program);
    //             });
    //     }
    // }


    void iRenderer::Update(float afTimeStep)
    {
        mfTimeCount += afTimeStep;
    }

    //-----------------------------------------------------------------------

    void iRenderer::AssignOcclusionObject(
        void* apSource, int alCustomIndex, iVertexBuffer* apVtxBuffer, cMatrixf* apMatrix, bool abDepthTest)
    {

        mpCurrentSettings->AssignOcclusionObject(this, apSource, alCustomIndex, apVtxBuffer, apMatrix, abDepthTest);
    }

    int iRenderer::RetrieveOcclusionObjectSamples(void* apSource, int alCustomIndex)
    {
        int lSamples = mpCurrentSettings->RetrieveOcclusionObjectSamples(this, apSource, alCustomIndex);

        return lSamples;
    }

    void iRenderer::WaitAndRetrieveAllOcclusionQueries()
    {
        mpCurrentSettings->WaitAndRetrieveAllOcclusionQueries(this);
    }

    //-----------------------------------------------------------------------

    //////////////////////////////////////////////////////////////////////////
    // PRIVATE METHODS
    //////////////////////////////////////////////////////////////////////////

    //-----------------------------------------------------------------------

    void iRenderer::BeginRendering(
        float afFrameTime,
        cFrustum* apFrustum,
        cWorld* apWorld,
        cRenderSettings* apSettings,
        bool abSendFrameBufferToPostEffects,
        bool abAtStartOfRendering)
    {
        if (apSettings->mbLog)
        {
            Log("-----------------  START -------------------------\n");
        }

        //////////////////////////////////////////
        // Set up variables
        mfCurrentFrameTime = afFrameTime;
        mpCurrentWorld = apWorld;
        mpCurrentSettings = apSettings;
        mpCurrentRenderList = apSettings->mpRenderList;

        mbSendFrameBufferToPostEffects = abSendFrameBufferToPostEffects;
        // mpCallbackList = apCallbackList;

        mbOcclusionPlanesActive = true;

        ////////////////////////////////
        // Initialize render functions
        InitAndResetRenderFunctions(
            apFrustum,
            apSettings->mbLog,
            apSettings->mbUseScissorRect,
            apSettings->mvScissorRectPos,
            apSettings->mvScissorRectSize);

        ////////////////////////////////
        // Set up near plane variables

        // Calculate radius for near plane so that it is always inside it.
        float fTanHalfFOV = tan(apFrustum->GetFOV() * 0.5f);

        float fNearPlane = apFrustum->GetNearPlane();
        mfCurrentNearPlaneTop = fTanHalfFOV * fNearPlane;
        mfCurrentNearPlaneRight = apFrustum->GetAspect() * mfCurrentNearPlaneTop;

        /////////////////////////////////////////////
        // Setup occlusion planes
        mvCurrentOcclusionPlanes.resize(0);


        // Fog
        if (mbSetupOcclusionPlaneForFog && apWorld->GetFogActive() && apWorld->GetFogColor().a >= 1.0f && apWorld->GetFogCulling())
        {
            cPlanef fogPlane;
            fogPlane.FromNormalPoint(apFrustum->GetForward(), apFrustum->GetOrigin() + apFrustum->GetForward() * -apWorld->GetFogEnd());
            mvCurrentOcclusionPlanes.push_back(fogPlane);
        }

        // if (mbSetFrameBufferAtBeginRendering && abAtStartOfRendering)
        // {
        // 	SetFrameBuffer(mpCurrentRenderTarget->mpFrameBuffer, true, true);
        // }

        // //////////////////////////////
        // // Clear screen
        // if (mbClearFrameBufferAtBeginRendering && abAtStartOfRendering)
        // {
        // 	// TODO: need to work out clearing framebuffer
        // 	ClearFrameBuffer(eClearFrameBufferFlag_Depth | eClearFrameBufferFlag_Color, true);
        // }

        //////////////////////////////////////////////////
        // Projection matrix
        // SetNormalFrustumProjection();

        /////////////////////////////////////////////
        // Clear Render list
        if (abAtStartOfRendering)
            mpCurrentRenderList->Clear();
    }

    cShadowMapData* iRenderer::GetShadowMapData(eShadowMapResolution aResolution, iLight* apLight)
    {
        ////////////////////////////
        // If size is 1, then just return that one
        if (m_shadowMapData[aResolution].size() == 1)
        {
            return &m_shadowMapData[aResolution][0];
        }

        //////////////////////////
        // Set up variables
        cShadowMapData* pBestData = NULL;
        int lMaxFrameDist = -1;

        ////////////////////////////
        // Iterate the shadow map array looking for shadow map already used by light
        // Else find the one with the largest frame length.
        for (size_t i = 0; i < m_shadowMapData[aResolution].size(); ++i)
        {
            cShadowMapData& pData = m_shadowMapData[aResolution][i];

            if (pData.mCache.mpLight == apLight)
            {
                pBestData = &pData;
                break;
            }
            else
            {
                int lFrameDist = cMath::Abs(pData.mlFrameCount - mlRenderFrameCount);
                if (lFrameDist > lMaxFrameDist)
                {
                    lMaxFrameDist = lFrameDist;
                    pBestData = &pData;
                }
            }
        }

        ////////////////////////////
        // Update the usage count
        if (pBestData)
        {
            pBestData->mlFrameCount = mlRenderFrameCount;
        }

        return pBestData;
    }

    bool iRenderer::ShadowMapNeedsUpdate(iLight* apLight, cShadowMapData* apShadowData)
    {
        // Occlusion culling must always be updated!
        if (apLight->GetOcclusionCullShadowCasters())
            return true;

        cShadowMapLightCache& cacheData = apShadowData->mCache;

        ///////////////////////////
        // Check if texture map and light are valid
        bool bValid = cacheData.mpLight == apLight && cacheData.mlTransformCount == apLight->GetTransformUpdateCount() &&
            cacheData.mfRadius == apLight->GetRadius();

        /////////////////////////////
        // Spotlight specific
        if (bValid && apLight->GetLightType() == eLightType_Spot)
        {
            cLightSpot* pSpotLight = static_cast<cLightSpot*>(apLight);
            bValid = pSpotLight->GetAspect() == cacheData.mfAspect && pSpotLight->GetFOV() == cacheData.mfFOV;
        }

        /////////////////////////////
        // Shadow casters
        if (bValid)
        {
            bValid = apLight->ShadowCastersAreUnchanged(mvShadowCasters);
        }

        /////////////////////////////
        // If not valid, update data
        if (bValid == false)
        {
            cacheData.SetFromLight(apLight);
            apLight->SetShadowCasterCacheFromVec(mvShadowCasters);
        }

        return bValid ? false : true;
    }

    void iRenderer::RenderZObject(GraphicsContext& context, iRenderable* apObject, cFrustum* apCustomFrustum)
    {
    }

    //-----------------------------------------------------------------------

    void iRenderer::CheckNodesAndAddToListIterative(iRenderableContainerNode* apNode, tRenderableFlag alNeededFlags)
    {
        ///////////////////////////////////////
        // Make sure node is updated
        apNode->UpdateBeforeUse();

        ///////////////////////////////////////
        // Get frustum collision, if previous was inside, then this is too!
        eCollision frustumCollision = mpCurrentFrustum->CollideNode(apNode);

        ////////////////////////////////
        // Do a visible check but always iterate the root node!
        if (apNode->GetParent())
        {
            if (frustumCollision == eCollision_Outside)
                return;
            if (CheckNodeIsVisible(apNode) == false)
                return;
        }

        ////////////////////////
        // Iterate children
        if (apNode->HasChildNodes())
        {
            tRenderableContainerNodeListIt childIt = apNode->GetChildNodeList()->begin();
            for (; childIt != apNode->GetChildNodeList()->end(); ++childIt)
            {
                iRenderableContainerNode* pChildNode = *childIt;
                CheckNodesAndAddToListIterative(pChildNode, alNeededFlags);
            }
        }

        /////////////////////////////
        // Iterate objects
        if (apNode->HasObjects())
        {
            tRenderableListIt it = apNode->GetObjectList()->begin();
            for (; it != apNode->GetObjectList()->end(); ++it)
            {
                iRenderable* pObject = *it;
                if (CheckObjectIsVisible(pObject, alNeededFlags) == false)
                    continue;

                if (frustumCollision == eCollision_Inside || pObject->CollidesWithFrustum(mpCurrentFrustum))
                {
                    mpCurrentRenderList->AddObject(pObject);
                }
            }
        }
    }

    //-----------------------------------------------------------------------

    void iRenderer::CheckForVisibleAndAddToList(iRenderableContainer* apContainer, tRenderableFlag alNeededFlags)
    {
        apContainer->UpdateBeforeRendering();

        CheckNodesAndAddToListIterative(apContainer->GetRoot(), alNeededFlags);
    }

    //-----------------------------------------------------------------------

    /**
     * Inserts the child nodes in apNode in a_setNodeStack.
     */
    void iRenderer::PushNodeChildrenToStack(tRendererSortedNodeSet& a_setNodeStack, iRenderableContainerNode* apNode, int alNeededFlags)
    {
        if (apNode->HasChildNodes() == false)
        {
            return;
        }

        tRenderableContainerNodeListIt childIt = apNode->GetChildNodeList()->begin();
        for (; childIt != apNode->GetChildNodeList()->end(); ++childIt)
        {
            iRenderableContainerNode* pChildNode = *childIt;

            ///////////////////////
            // Make sure node is update
            pChildNode->UpdateBeforeUse();

            ///////////////////////////////////////////////////
            // Check node has object and needed flags
            if (pChildNode->UsesFlagsAndVisibility() &&
                (pChildNode->HasVisibleObjects() == false || (pChildNode->GetRenderFlags() & alNeededFlags) != alNeededFlags))
            {
                continue;
            }

            ///////////////////////////////////////////////////
            // Check if inside frustum, skipping test if parent was inside
            eCollision frustumCollision =
                apNode->GetPrevFrustumCollision() == eCollision_Inside ? eCollision_Inside : mpCurrentFrustum->CollideNode(pChildNode);

            if (frustumCollision == eCollision_Outside)
                continue;
            if (CheckNodeIsVisible(pChildNode) == false)
                continue;

            pChildNode->SetPrevFrustumCollision(frustumCollision);

            ////////////////////////////////////////////////////////
            // Check if the frustum origin is inside the node AABB.
            //  If so no intersection test is needed
            if (mpCurrentFrustum->CheckAABBNearPlaneIntersection(pChildNode->GetMin(), pChildNode->GetMax()))
            {
                cVector3f vViewSpacePos = cMath::MatrixMul(mpCurrentFrustum->GetViewMatrix(), pChildNode->GetCenter());
                pChildNode->SetViewDistance(vViewSpacePos.z);
                pChildNode->SetInsideView(true);
            }
            ////////////////////////////////////////////////////////
            // Frustum origin is outside of node. Do intersection test.
            else
            {
                cVector3f vIntersection;
                cMath::CheckAABBLineIntersection(
                    pChildNode->GetMin(),
                    pChildNode->GetMax(),
                    mpCurrentFrustum->GetOrigin(),
                    pChildNode->GetCenter(),
                    &vIntersection,
                    NULL);
                cVector3f vViewSpacePos = cMath::MatrixMul(mpCurrentFrustum->GetViewMatrix(), vIntersection);
                pChildNode->SetViewDistance(vViewSpacePos.z);
                pChildNode->SetInsideView(false);
            }

            ////////////////////////////////////////////////////////
            // Add to sorted stack.
            a_setNodeStack.insert(pChildNode);
        }
    }

    //-----------------------------------------------------------------------

    void iRenderer::AddAndRenderNodeOcclusionQuery(tNodeOcclusionPairList* apList, iRenderableContainerNode* apNode, bool abObjectsRendered)
    {
        // //DEBUG
        // // Skip using a query and just render the node.
        // if(	mbOnlyRenderPrevVisibleOcclusionObjects &&
        // 	mlOnlyRenderPrevVisibleOcclusionObjectsFrameCount >= 1)
        // {
        // 	RenderNodeBoundingBox(apNode,NULL);
        // 	return;
        // }

        // //////////////////////
        // //Create the pair
        // cNodeOcclusionPair noPair;
        // noPair.mpNode = apNode;
        // noPair.mpQuery = GetOcclusionQuery();
        // noPair.mbObjectsRendered = abObjectsRendered;

        // if(mbLog)Log("CHC: Testing query %d on node: %d\n",noPair.mpQuery, apNode);

        // /////////////////////
        // // Render node AABB
        // RenderNodeBoundingBox(apNode, noPair.mpQuery);

        // /////////////////////
        // // Add to list
        // apList->push_back(noPair);
    }


    cVisibleRCNodeTracker* gpCurrentVisibleNodeTracker = NULL;
    void iRenderer::PushUpVisibility(iRenderableContainerNode* apNode)
    {
        gpCurrentVisibleNodeTracker->SetNodeVisible(apNode);

        if (apNode->GetParent())
            PushUpVisibility(apNode->GetParent());
    }

    void iRenderer::OcclusionQueryBoundingBoxTest(
        bgfx::ViewId view,
        GraphicsContext& context,
        bgfx::OcclusionQueryHandle handle,
        const cFrustum& frustum,
        const cMatrixf& transform,
        RenderTarget& rt,
        Cull cull)
    {
        GraphicsContext::ShaderProgram shaderProgram;
        GraphicsContext::LayoutStream layoutStream;
        mpShapeBox->GetLayoutStream(layoutStream);

        shaderProgram.m_handle = m_nullShader;
        shaderProgram.m_modelTransform = transform.GetTranspose();
        shaderProgram.m_configuration.m_depthTest = DepthTest::Less;
        shaderProgram.m_configuration.m_cull = cull;

        GraphicsContext::DrawRequest drawRequest{ layoutStream, shaderProgram };
        context.Submit(view, drawRequest);
    }

    void iRenderer::RenderZPassWithVisibilityCheck(
        GraphicsContext& context,
        cVisibleRCNodeTracker* apVisibleNodeTracker,
        tObjectVariabilityFlag objectTypes,
        tRenderableFlag alNeededFlags,
        RenderTarget& rt,
        std::function<bool(bgfx::ViewId view, iRenderable* object)> renderHandler)
    {
        auto imageSize = rt.GetImage()->GetImageSize();
        GraphicsContext::ViewConfiguration viewConfiguration {rt};
        viewConfiguration.m_projection = mpCurrentFrustum->GetProjectionMatrix().GetTranspose();
        viewConfiguration.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
        viewConfiguration.m_viewRect = {0, 0, static_cast<uint16_t>(imageSize.x), static_cast<uint16_t>(imageSize.y)};
        auto pass = context.StartPass("z pass", viewConfiguration);
        
        auto renderNodeHandler = [&](iRenderableContainerNode* apNode, tRenderableFlag alNeededFlags)
        {
            if (apNode->HasObjects() == false)
                return 0;

            int lRenderedObjects = 0;

            ////////////////////////////////////
            // Iterate sorted objects and render
            for (tRenderableListIt it = apNode->GetObjectList()->begin(); it != apNode->GetObjectList()->end(); ++it)
            {
                iRenderable* pObject = *it;

                //////////////////////
                // Check if object is visible
                if (CheckObjectIsVisible(pObject, alNeededFlags) == false)
                    continue;

                /////////////////////////////
                // Check if inside frustum, skip test node was inside
                if (apNode->GetPrevFrustumCollision() != eCollision_Inside && pObject->CollidesWithFrustum(mpCurrentFrustum) == false)
                {
                    continue;
                }

                if (renderHandler(pass, pObject))
                {
                    ++lRenderedObjects;
                }
            }

            return lRenderedObjects;
        };

        struct
        {
            iRenderableContainer* container;
            tObjectVariabilityFlag flag;
        } containers[] = {
            { mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Static), eObjectVariabilityFlag_Static },
            { mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Dynamic), eObjectVariabilityFlag_Dynamic },
        };

        mpCurrentSettings->mlNumberOfOcclusionQueries = 0;

        // Switch sets, so that the last frames set is not current.
        apVisibleNodeTracker->SwitchAndClearVisibleNodeSet();

        for (auto& it : containers)
        {
            if (it.flag & objectTypes)
            {
                it.container->UpdateBeforeRendering();
            }
        }

        // Temp variable used when pushing visibility
        gpCurrentVisibleNodeTracker = apVisibleNodeTracker;

        // Add Root nodes to stack
        tRendererSortedNodeSet setNodeStack;

        for (auto& it : containers)
        {
            if (it.flag & objectTypes)
            {
                iRenderableContainerNode* pNode = it.container->GetRoot();
                pNode->UpdateBeforeUse(); // Make sure node is updated.
                pNode->SetInsideView(true); // We never want to check root! Assume player is inside.
                setNodeStack.insert(pNode);
            }
        }

        // Render at least X objects without rendering nodes, to some occluders
        int lMinRenderedObjects = mpCurrentSettings->mlMinimumObjectsBeforeOcclusionTesting;

        ////////////////////////////
        // Iterate the nodes on the stack.
        while (!setNodeStack.empty())
        {
            tRendererSortedNodeSetIt firstIt = setNodeStack.begin(); // Might be slow...
            iRenderableContainerNode* pNode = *firstIt;
            setNodeStack.erase(firstIt); // This is most likely very slow... use list?

            //////////////////////////
            // Check if node is a leaf
            bool bNodeIsLeaf = pNode->HasChildNodes() == false;

            //////////////////////////
            // Check if near plane is inside node AABB
            bool bNearPlaneInsideNode = pNode->IsInsideView();

            //////////////////////////
            // Check if node was visible
            bool bWasVisible = apVisibleNodeTracker->WasNodeVisible(pNode);

            auto occlusionResult = bgfx::OcclusionQueryResult::Visible; // TODO: occlusion queries are broken

            //////////////////////////
            // Render nodes and queue queries
            //  Only do this if:
            //  - Near plane is not inside node AABB
            //  - All of the closest objects have been rendered (so there are some blockers)
            //  - Node was not visible or node is leaf (always draw leaves!)
            if (bNearPlaneInsideNode == false && lMinRenderedObjects <= 0 &&
                (bWasVisible == false || bNodeIsLeaf || occlusionResult == bgfx::OcclusionQueryResult::Visible))
            {
                ////////////////
                // If node is leaf and was visible render objects directly.
                //  Rendering directly can be good as it may result in extra blocker for next node test.
                bool bRenderObjects = false;
                if (bNodeIsLeaf && bWasVisible)
                {
                    bRenderObjects = true;
                }

                /////////////////////////
                // Matrix
                cVector3f vSize = pNode->GetMax() - pNode->GetMin();
                cMatrixf mtxBox = cMath::MatrixScale(vSize);
                mtxBox.SetTranslation(pNode->GetCenter());
                // SetModelViewMatrix( cMath::MatrixMul(mpCurrentFrustum->GetViewMatrix(), mtxBox) );
                OcclusionQueryBoundingBoxTest(pass, context, pNode->GetOcclusionQuery(), *mpCurrentFrustum, mtxBox, rt);

                //////////////////////////
                // Render node objects after AABB so that an object does not occlude its own node.
                if (bRenderObjects || occlusionResult == bgfx::OcclusionQueryResult::Visible)
                {
                    renderNodeHandler(pNode, alNeededFlags);
                }

                // Debug:
                //  Skipping any queries, so must push up visible if this node was visible
                if ((mbOnlyRenderPrevVisibleOcclusionObjects && mlOnlyRenderPrevVisibleOcclusionObjectsFrameCount >= 1 && bRenderObjects &&
                     bWasVisible) ||
                    occlusionResult == bgfx::OcclusionQueryResult::Visible)
                {
                    PushUpVisibility(pNode);
                }

                if (occlusionResult == bgfx::OcclusionQueryResult::Visible)
                {
                    PushNodeChildrenToStack(setNodeStack, pNode, alNeededFlags);
                }
            }
            //////////////////////////
            // If previous test failed, push children to stack and render objects (if any) directly.
            else
            {
                ////////////////
                // Add child nodes to stack if any (also checks if they have needed flags are in frustum)
                PushNodeChildrenToStack(setNodeStack, pNode, alNeededFlags);

                ////////////////
                // Render objects if any
                int lObjectsRendered = renderNodeHandler(pNode, alNeededFlags);
                lMinRenderedObjects -= lObjectsRendered;
            }
        }
    }

    //-----------------------------------------------------------------------

    static tRenderableVec* gpLightShadowCasterVec = NULL;
    static cFrustum* gpLightFrustum = NULL;
    static cFrustum* gpViewFrustum = NULL;
    static eFrustumPlane gvBeyondLightAndViewPlanes[6]; // Use to check if an caster is not going to affect view.
    static int glBeyondLightAndViewPlaneNum;
    static bool gbLightBehindNearPlane;

    //-----------------------------------------------------------------------

    static bool BoxIntersectOrInsidePlane(const cPlanef& aPlane, cVector3f* apCornerVec)
    {
        for (int corner = 0; corner < 8; ++corner)
        {
            // Check if point is inside, if so skip plane...
            if (cMath::PlaneToPointDist(aPlane, apCornerVec[corner]) > 0)
            {
                return true;
            }
        }

        return false;
    }

    static bool BoxInsidePlane(const cPlanef& aPlane, cVector3f* apCornerVec)
    {
        for (int corner = 0; corner < 8; ++corner)
        {
            // Check if point is inside, if so skip plane...
            if (cMath::PlaneToPointDist(aPlane, apCornerVec[corner]) < 0)
            {
                return false;
            }
        }

        return true;
    }

    //-----------------------------------------------------------------------

    bool iRenderer::CheckShadowCasterContributesToView(iRenderable* apObject)
    {
        // This should be always true since the shadow map might be saved and will then end up faulty if some objects have been dismissed!
        return true;

        cBoundingVolume* pBV = apObject->GetBoundingVolume();

        // Log("---- Checking %s --- \n",apObject->GetName().c_str());

        // Log("1\n");
        //////////////////////////////////////
        // If light is behind near plane, one must check if object in front of near plane
        bool bObjectMightOnNearPlane = false;
        if (gbLightBehindNearPlane)
        {
            return true; // Temp, until I can come up with a qay to resolve the issues.

            const cPlanef& nearPlane = gpViewFrustum->GetPlane(eFrustumPlane_Near);
            float fDist = cMath::PlaneToPointDist(nearPlane, pBV->GetWorldCenter());

            // Object is outside of near plane.
            if (fDist < -pBV->GetRadius())
                return true;

            // Object is not really inside near plane, test more accurately later
            if (fDist < pBV->GetRadius())
            {
                bObjectMightOnNearPlane = true; // Do not test yet
            }
        }

        eCollision frustumCollision = eCollision_Inside;

        // Log("2\n");
        ////////////////////////////
        // Sphere test
        int lOutsideCount = 0;
        int lInsideCount = 0;
        for (int i = 0; i < glBeyondLightAndViewPlaneNum; ++i)
        {
            eFrustumPlane frustumPlane = gvBeyondLightAndViewPlanes[i];
            const cPlanef& cameraPlane = gpViewFrustum->GetPlane(frustumPlane);

            float fDist = cMath::PlaneToPointDist(cameraPlane, pBV->GetWorldCenter());

            // Test if inside of plane
            if (fDist > pBV->GetRadius())
            {
                lInsideCount++;
            }
            else if (fDist < -pBV->GetRadius())
            {
                lOutsideCount++;
                break;
            }
        }

        // Log("3\n");
        //  If all was inside, then we are sure it contributes
        if (lInsideCount == glBeyondLightAndViewPlaneNum)
            return true;

        // Log("4\n");
        //  If any was outside, we are sure it does NOT contribute
        if (lOutsideCount > 0 && bObjectMightOnNearPlane == false)
            return false;

        ////////////////////
        // Create corners
        const cVector3f& vMax = pBV->GetMax();
        const cVector3f& vMin = pBV->GetMin();

        cVector3f vCorners[8] = {
            cVector3f(vMax.x, vMax.y, vMax.z), cVector3f(vMax.x, vMax.y, vMin.z),
            cVector3f(vMax.x, vMin.y, vMax.z), cVector3f(vMax.x, vMin.y, vMin.z),

            cVector3f(vMin.x, vMax.y, vMax.z), cVector3f(vMin.x, vMax.y, vMin.z),
            cVector3f(vMin.x, vMin.y, vMax.z), cVector3f(vMin.x, vMin.y, vMin.z),
        };

        // Log("5\n");
        ////////////////////////////
        // Check near plane intersection
        if (lOutsideCount > 0 && bObjectMightOnNearPlane)
        {
            // If object is not fully inside, it contributes
            if (BoxInsidePlane(gpViewFrustum->GetPlane(eFrustumPlane_Near), vCorners) == false)
            {
                return true;
            }
            bObjectMightOnNearPlane = false; // Reset so it is not tested again.
        }

        ////////////////////////////
        // Box Test

        // Log("6\n");
        // If needed check so that it is inside the near plane
        if (bObjectMightOnNearPlane)
        {
            // If object is not fully inside, it contributes
            if (BoxInsidePlane(gpViewFrustum->GetPlane(eFrustumPlane_Near), vCorners) == false)
            {
                return true;
            }
        }

        // Log("7\n");
        // Iterate all planes separating between contributing and not.
        bool bContributes;
        for (int plane = 0; plane < glBeyondLightAndViewPlaneNum; ++plane)
        {
            eFrustumPlane frustumPlane = gvBeyondLightAndViewPlanes[plane];
            const cPlanef& cameraPlane = gpViewFrustum->GetPlane(frustumPlane);

            bContributes = BoxIntersectOrInsidePlane(cameraPlane, vCorners);

            // Log("plane %d contribute: %d\n",frustumPlane, bContributes);

            if (bContributes == false)
                break;
        }

        return bContributes;
    }

    //-----------------------------------------------------------------------

    void iRenderer::GetShadowCastersIterative(iRenderableContainerNode* apNode, eCollision aPrevCollision)
    {
        ///////////////////////////////////////
        // Make sure node is updated
        apNode->UpdateBeforeUse();

        ///////////////////////////////////////
        // Get frustum collision, if previous was inside, then this is too!
        eCollision frustumCollision = aPrevCollision == eCollision_Inside ? aPrevCollision : gpLightFrustum->CollideNode(apNode);

        ///////////////////////////////////
        // Check if visible but always iterate the root node!
        if (apNode->GetParent())
        {
            if (frustumCollision == eCollision_Outside)
                return;
            if (CheckNodeIsVisible(apNode) == false)
                return;
        }

        ////////////////////////
        // Iterate children
        if (apNode->HasChildNodes())
        {
            for (tRenderableContainerNodeListIt childIt = apNode->GetChildNodeList()->begin(); childIt != apNode->GetChildNodeList()->end();
                 ++childIt)
            {
                iRenderableContainerNode* pChildNode = *childIt;
                GetShadowCastersIterative(pChildNode, frustumCollision);
            }
        }

        /////////////////////////////
        // Iterate objects
        if (apNode->HasObjects())
        {
            for (tRenderableListIt it = apNode->GetObjectList()->begin(); it != apNode->GetObjectList()->end(); ++it)
            {
                iRenderable* pObject = *it;

                /////////
                // Check so visible and shadow caster
                if (CheckObjectIsVisible(pObject, eRenderableFlag_ShadowCaster) == false || pObject->GetMaterial() == NULL ||
                    pObject->GetMaterial()->GetType()->IsTranslucent())
                {
                    continue;
                }

                /////////
                // Check if in frustum
                if (frustumCollision != eCollision_Inside &&
                    gpLightFrustum->CollideBoundingVolume(pObject->GetBoundingVolume()) == eCollision_Outside)
                {
                    continue;
                }

                /////////
                // Check if it contributes to scene
                if (CheckShadowCasterContributesToView(pObject) == false)
                    continue;

                ///////////////////////////////
                // Add object!

                // Calculate the view space Z (just a squared distance)
                pObject->SetViewSpaceZ(cMath::Vector3DistSqr(pObject->GetBoundingVolume()->GetWorldCenter(), gpLightFrustum->GetOrigin()));

                // Add to list
                gpLightShadowCasterVec->push_back(pObject);
            }
        }
    }

    //-----------------------------------------------------------------------

    void iRenderer::GetShadowCasters(iRenderableContainer* apContainer, tRenderableVec& avObjectVec, cFrustum* apLightFrustum)
    {
        apContainer->UpdateBeforeRendering();

        gpLightFrustum = apLightFrustum;
        gpLightShadowCasterVec = &avObjectVec;

        GetShadowCastersIterative(apContainer->GetRoot(), eCollision_Outside);
    }

    //-----------------------------------------------------------------------

    static bool SortFunc_ShadowCasters(iRenderable* apObjectA, iRenderable* apObjectB)
    {
        cMaterial* pMatA = apObjectA->GetMaterial();
        cMaterial* pMatB = apObjectB->GetMaterial();

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
        return apObjectA->GetViewSpaceZ() < apObjectB->GetViewSpaceZ();
    }

    //-----------------------------------------------------------------------

    bool iRenderer::SetupShadowMapRendering(iLight* apLight)
    {
        /////////////////////////
        // Get light data
        if (apLight->GetLightType() != eLightType_Spot)
            return false; // Only support spot lights for now...

        cLightSpot* pSpotLight = static_cast<cLightSpot*>(apLight);
        cFrustum* pLightFrustum = pSpotLight->GetFrustum();

        // Set the view frustum, needed in some functions cause the current is set for the light during rendering.
        gpViewFrustum = mpCurrentFrustum;

        /////////////////////////
        // Get the camera planes that face away from the light
        glBeyondLightAndViewPlaneNum = 0;
        cVector3f vLightForward = pLightFrustum->GetForward();
        for (int i = 0; i < eFrustumPlane_LastEnum; ++i)
        {
            const cPlanef& cameraPlane = gpViewFrustum->GetPlane((eFrustumPlane)i);

            // Check so plane is facing the light
            //  Above 0 because GetForward from frustum is inverted.
            //  Note that this is not optimal, but good enough for now. Should have some other way of choosing to make it better.
            cVector3f vPlaneNormal = cameraPlane.GetNormal();
            if (cMath::Vector3Dot(vPlaneNormal, vLightForward) > 0)
            {
                gvBeyondLightAndViewPlanes[glBeyondLightAndViewPlaneNum] = (eFrustumPlane)i;
                ++glBeyondLightAndViewPlaneNum;
            }
        }

        /////////////////////////
        // See if light is behind near plane
        if (cMath::PlaneToPointDist(gpViewFrustum->GetPlane(eFrustumPlane_Near), pLightFrustum->GetOrigin()) < 0)
        {
            gbLightBehindNearPlane = true;
        }
        else
        {
            gbLightBehindNearPlane = false;
        }

        /////////////////////////
        // If culling by occlusion, skip rest of function
        if (apLight->GetOcclusionCullShadowCasters())
        {
            // mvShadowCasters.resize(0); //Debug reasons only, remove later!
            return true;
        }

        /////////////////////////
        // Get objects to render

        // Clear list
        mvShadowCasters.resize(0); // No clear, so we keep all in memory.

        // Get the objects
        if (apLight->GetShadowCastersAffected() & eObjectVariabilityFlag_Dynamic)
            GetShadowCasters(
                mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Dynamic), mvShadowCasters, pSpotLight->GetFrustum());

        if (apLight->GetShadowCastersAffected() & eObjectVariabilityFlag_Static)
            GetShadowCasters(mpCurrentWorld->GetRenderableContainer(eWorldContainerType_Static), mvShadowCasters, pSpotLight->GetFrustum());

        // See if any objects where added.
        if (mvShadowCasters.empty())
            return false;

        // Sort the list
        std::sort(mvShadowCasters.begin(), mvShadowCasters.end(), SortFunc_ShadowCasters);

        return true;
    }

    //-----------------------------------------------------------------------

    static cFrustum* gpTempLightFrustum = NULL;

    static bool SortFunc_OcclusionObject(cOcclusionQueryObject* apObjectA, cOcclusionQueryObject* apObjectB)
    {
        //////////////////////////
        // Vertex buffer
        if (apObjectA->mpVtxBuffer != apObjectB->mpVtxBuffer)
        {
            return apObjectA->mpVtxBuffer < apObjectB->mpVtxBuffer;
        }

        //////////////////////////
        // Depth
        if (apObjectA->mbDepthTest != apObjectB->mbDepthTest)
        {
            return apObjectA->mbDepthTest;
        }

        //////////////////////////
        // Matrix
        return apObjectA->mpMatrix < apObjectB->mpMatrix;
    }

    void iRenderer::AssignAndRenderOcclusionQueryObjects(bgfx::ViewId view, GraphicsContext& context, bool abSetFrameBuffer, bool abUsePosAndSize)
    {
        cRenderList* pRenderList = mpCurrentSettings->mpRenderList;

        ///////////////////////////////////
        // Get and use any previous occlusion queries
        for (auto& pObject: pRenderList->GetOcclusionQueryItems())
        {
            pObject->ResolveOcclusionPass(
                this,
                [&](bgfx::OcclusionQueryHandle handle,
                    DepthTest depth,
                    GraphicsContext::LayoutStream& layoutStream,
                    const cMatrixf& transformMatrix)
                {
                    GraphicsContext::ShaderProgram shaderProgram;
                    shaderProgram.m_handle = m_nullShader;
                    shaderProgram.m_configuration.m_depthTest = depth;
                    shaderProgram.m_configuration.m_cull = Cull::CounterClockwise;

                    shaderProgram.m_modelTransform = transformMatrix;
                    // shaderProgram.m_view = mpCurrentFrustum->GetViewMatrix().GetTranspose();
                    // shaderProgram.m_projection = mpCurrentProjectionMatrix->GetTranspose();

                    GraphicsContext::DrawRequest drawRequest{ layoutStream, shaderProgram };
                    context.Submit(view, drawRequest, handle);
                });
        }
    }

    bool iRenderer::CheckRenderablePlaneIsVisible(iRenderable* apObject, cFrustum* apFrustum)
    {
        if (apObject->GetRenderType() != eRenderableType_SubMesh)
            return true;

        cSubMeshEntity* pSubMeshEnt = static_cast<cSubMeshEntity*>(apObject);
        cSubMesh* pSubMesh = pSubMeshEnt->GetSubMesh();

        if (pSubMesh->GetIsOneSided() == false)
            return true;

        cVector3f vNormal = cMath::MatrixMul3x3(apObject->GetWorldMatrix(), pSubMesh->GetOneSidedNormal());
        cVector3f vPos = cMath::MatrixMul(apObject->GetWorldMatrix(), pSubMesh->GetOneSidedPoint());

        float fDot = cMath::Vector3Dot(vPos - apFrustum->GetOrigin(), vNormal);

        return fDot < 0;
    }

    //-----------------------------------------------------------------------

    cRect2l iRenderer::GetClipRectFromObject(
        iRenderable* apObject, float afPaddingPercent, cFrustum* apFrustum, const cVector2l& avScreenSize, float afHalfFovTan)
    {
        cBoundingVolume* pBV = apObject->GetBoundingVolume();

        cRect2l clipRect;
        if (afHalfFovTan == 0)
            afHalfFovTan = tan(apFrustum->GetFOV() * 0.5f);
        cMath::GetClipRectFromBV(clipRect, *pBV, apFrustum, avScreenSize, afHalfFovTan);

        // Add 20% padding on clip rect
        int lXInc = (int)((float)clipRect.w * afHalfFovTan);
        int lYInc = (int)((float)clipRect.h * afHalfFovTan);

        clipRect.x = cMath::Max(clipRect.x - lXInc, 0);
        clipRect.y = cMath::Max(clipRect.y - lYInc, 0);
        clipRect.w = cMath::Min(clipRect.w + lXInc * 2, avScreenSize.x - clipRect.x);
        clipRect.h = cMath::Min(clipRect.h + lYInc * 2, avScreenSize.y - clipRect.y);

        return clipRect;
    }

    //-----------------------------------------------------------------------

    bool iRenderer::CheckObjectIsVisible(iRenderable* apObject, tRenderableFlag alNeededFlags)
    {
        /////////////////////////////
        // Is Visible var
        if (apObject->IsVisible() == false)
            return false;

        /////////////////////////////
        // Check flags
        if ((apObject->GetRenderFlags() & alNeededFlags) != alNeededFlags)
            return false;

        /////////////////////////////
        // Clip plane check.
        // NOTE: This shall always be the user clip planes! Since we wanna cull stuff like nodes where clip planes are not active, etc.
        if (mvCurrentOcclusionPlanes.empty() == false && mbOcclusionPlanesActive)
        {
            cBoundingVolume* pBV = apObject->GetBoundingVolume();
            for (size_t i = 0; i < mvCurrentOcclusionPlanes.size(); ++i)
            {
                cPlanef& plane = mvCurrentOcclusionPlanes[i];

                if (cMath::CheckPlaneBVCollision(plane, *pBV) == eCollision_Outside)
                    return false;
            }
        }

        return true;
    }

    //-----------------------------------------------------------------------

    bool iRenderer::CheckNodeIsVisible(iRenderableContainerNode* apNode)
    {
        if (mbOcclusionPlanesActive == false || mvCurrentOcclusionPlanes.empty())
            return true;

        // NOTE: This shall always be the user clip planes! The render function ones might not be active when culling is needed and so on.
        for (size_t i = 0; i < mvCurrentOcclusionPlanes.size(); ++i)
        {
            cPlanef& plane = mvCurrentOcclusionPlanes[i];

            if (cMath::CheckPlaneAABBCollision(plane, apNode->GetMin(), apNode->GetMax(), apNode->GetCenter(), apNode->GetRadius()) ==
                eCollision_Outside)
            {
                return false;
            }
        }

        return true;
    }

    void iRenderer::SetOcclusionPlanesActive(bool abX)
    {
        mbOcclusionPlanesActive = abX;
    }



} // namespace hpl
