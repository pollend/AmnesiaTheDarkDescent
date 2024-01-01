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

#include "graphics/Enum.h"
#include "graphics/RenderTarget.h"
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
    bool iRenderer::mbRefractionEnabled = true;
    int iRenderer::mlRenderFrameCount = 0;

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
        // Set up General Variables
        mbIsReflection = abIsReflection;

        mClearColor = cColor(0, 0);

        // Set up Render Variables
        // Change this later I assume:
        mlMinimumObjectsBeforeOcclusionTesting = 0; // 8;//8 should be good default, giving a good amount of colliders, or? Clarifiction:
                                                    // Minium num of object rendered until node visibility tests start!
        mlSampleVisiblilityLimit = 3;

        mbUseEdgeSmooth = false;

        mMaxShadowMapResolution = eShadowMapResolution_High;
        if (mbIsReflection) {
            mMaxShadowMapResolution = eShadowMapResolution_Medium;
        }

        mbClipReflectionScreenRect = true;

        mbRenderWorldReflection = true;

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

    }

    cRenderSettings::~cRenderSettings()
    {
    }

    //-----------------------------------------------------------------------

    void cRenderSettings::ResetVariables()
    {
    }


    iRenderer::iRenderer(const tString& asName, cGraphics* apGraphics, cResources* apResources)
    {
        mpResources = apResources;
        mfTimeCount = 0;
    }

    //-----------------------------------------------------------------------

    iRenderer::~iRenderer()
    {

    }


    void iRenderer::Update(float afTimeStep)
    {
        mfTimeCount += afTimeStep;
    }

    void iRenderer::BeginRendering(
        float afFrameTime,
        cFrustum* apFrustum,
        cWorld* apWorld,
        cRenderSettings* apSettings,
        bool abSendFrameBufferToPostEffects,
        bool abAtStartOfRendering)
    {
        //////////////////////////////////////////
        // Set up variables
        mfCurrentFrameTime = afFrameTime;
        mpCurrentSettings = apSettings;
    }

} // namespace hpl
