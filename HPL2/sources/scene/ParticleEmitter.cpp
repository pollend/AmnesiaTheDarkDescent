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

#include "scene/ParticleEmitter.h"

#include "impl/LegacyVertexBuffer.h"
#include "system/LowLevelSystem.h"

#include "resources/Resources.h"
#include "resources/MaterialManager.h"

#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include "graphics/Renderer.h"
#include <graphics/GraphicsBuffer.h>

#include "scene/Camera.h"
#include "math/Math.h"

#include "engine/Engine.h"
#include "scene/Scene.h"
#include "scene/World.h"

#include "scene/ParticleSystem.h"


namespace hpl {

    iParticleEmitterData::iParticleEmitterData(const tString& asName, cResources* apResources, cGraphics* apGraphics) {
        msName = asName;
        mpResources = apResources;
        mpGraphics = apGraphics;

        mfWarmUpTime = 0;
        mfWarmUpStepsPerSec = 20;
    }

    //-----------------------------------------------------------------------

    iParticleEmitterData::~iParticleEmitterData() {
        for (int i = 0; i < (int)mvMaterials.size(); i++) {
            if (mvMaterials[i])
                mpResources->GetMaterialManager()->Destroy(mvMaterials[i]);
        }
    }
    //-----------------------------------------------------------------------

    void iParticleEmitterData::AddMaterial(cMaterial* apMaterial) {
        mvMaterials.push_back(apMaterial);
    }

    iParticleEmitter::iParticleEmitter(
        tString asName,
        tMaterialVec* avMaterials,
        unsigned int alMaxParticles,
        cVector3f avSize,
        cGraphics* apGraphics,
        cResources* apResources)
        : iRenderable(asName) {
        mpGraphics = apGraphics;
        mpResources = apResources;

        // Create and set up particle data
        mvParticles.resize(alMaxParticles);
        for (int i = 0; i < (int)alMaxParticles; i++) {
            mvParticles[i] = hplNew(cParticle, ());
        }
        mlMaxParticles = alMaxParticles;
        mlNumOfParticles = 0;

        mvMaterials = avMaterials;

        auto* graphicsAllocator = Interface<GraphicsAllocator>::Get();
        auto& particleSet = graphicsAllocator->resolveSet(GraphicsAllocator::AllocationSet::ParticleSet);
        m_geometry = particleSet.allocate(4 * mlMaxParticles * NumberActiveCopies, 6 * mlMaxParticles);

        auto positionStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_POSITION);
        auto colorStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_COLOR);
        auto uvStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_TEXCOORD0);

        auto& indexStream = m_geometry->indexBuffer();

        BufferUpdateDesc indexUpdateDesc = { indexStream.m_handle,
                                             m_geometry->indexOffset() * GeometrySet::IndexBufferStride,
                                             GeometrySet::IndexBufferStride * 6 };
        BufferUpdateDesc positionUpdateDesc = { positionStream->buffer().m_handle,
                                                (m_geometry->vertexOffset() * positionStream->stride()),
                                                positionStream->stride() * 4 * mlMaxParticles * NumberActiveCopies };
        BufferUpdateDesc colorUpdateDesc = { colorStream->buffer().m_handle,
                                             (m_geometry->vertexOffset() * colorStream->stride()),
                                             colorStream->stride() * 4 * mlMaxParticles * NumberActiveCopies };
        BufferUpdateDesc uvUpdateDesc = { uvStream->buffer().m_handle,
                                          (m_geometry->vertexOffset() * uvStream->stride()),
                                          uvStream->stride() * 4 * mlMaxParticles * NumberActiveCopies };

        beginUpdateResource(&indexUpdateDesc);
        beginUpdateResource(&positionUpdateDesc);
        beginUpdateResource(&colorUpdateDesc);
        beginUpdateResource(&uvUpdateDesc);

        GraphicsBuffer gpuIndexBuffer(indexUpdateDesc);

        auto indexView = gpuIndexBuffer.CreateIndexView();
        uint32_t appendIdx = 0;
        for (size_t i = 0; i < alMaxParticles; i++) {
            for (int j = 0; j < 3; j++) {
                indexView.Write(appendIdx++, (i * 4) + j);
            }
            for (int j = 2; j < 5; j++) {
                indexView.Write(appendIdx++, (i * 4) + (j == 4 ? 0 : j));
            }
        }

        GraphicsBuffer gpuPositionBuffer(positionUpdateDesc);
        GraphicsBuffer gpuColorBuffer(colorUpdateDesc);
        GraphicsBuffer gpuUvBuffer(uvUpdateDesc);

        auto positionView = gpuPositionBuffer.CreateStructuredView<float3>();
        auto colorView = gpuColorBuffer.CreateStructuredView<float4>();
        auto uvView = gpuUvBuffer.CreateStructuredView<float2>();

        for (size_t i = 0; i < mlMaxParticles * NumberActiveCopies; i++) {
            uvView.Write((i * 4) + 0, float2(1, 1));
            uvView.Write((i * 4) + 1, float2(0, 1));
            uvView.Write((i * 4) + 2, float2(0, 0));
            uvView.Write((i * 4) + 3, float2(1, 0));
        }
        for (size_t i = 0; i < mlMaxParticles * NumberActiveCopies * 4; i++) {
            colorView.Write(i, float4(1, 1, 1, 1));
            positionView.Write(i, float3(0, 0, 0));
        }

        endUpdateResource(&indexUpdateDesc);
        endUpdateResource(&positionUpdateDesc);
        endUpdateResource(&colorUpdateDesc);
        endUpdateResource(&uvUpdateDesc);

        ////////////////////////////////////
        // Setup vars
        mlSleepCount =
            60 * 5; // Start with high sleep count to make sure a looping particle system reaches equilibrium. 5 secs should eb enough!

        mbDying = false;
        mfFrame = 0;

        mbUpdateGfx = true;
        mbUpdateBV = true;

        mlDirectionUpdateCount = -1;
        mvDirection = cVector3f(0, 0, 0);

        mvMaxDrawSize = 0;

        mlAxisDrawUpdateCount = -1;

        mbApplyTransformToBV = false;

        mBoundingVolume.SetSize(0);
        mBoundingVolume.SetPosition(0);

        mDrawType = eParticleEmitterType_FixedPoint;
        mCoordSystem = eParticleEmitterCoordSystem_World;

        mbUsesDirection = false; // If Direction should be udpdated
    }

    iParticleEmitter::~iParticleEmitter() {
        for (int i = 0; i < (int)mvParticles.size(); i++) {
            hplDelete(mvParticles[i]);
        }
    }

    void iParticleEmitter::SetSubDivUV(const cVector2l& avSubDiv) {
        // Check so that there is any subdivision and that no sub divison axis is
        // equal or below zero
        if ((avSubDiv.x > 1 || avSubDiv.x > 1) && (avSubDiv.x > 0 && avSubDiv.y > 0)) {
            int lSubDivNum = avSubDiv.x * avSubDiv.y;

            mvSubDivUV.resize(lSubDivNum);

            float fInvW = 1.0f / (float)avSubDiv.x;
            float fInvH = 1.0f / (float)avSubDiv.y;

            for (int x = 0; x < avSubDiv.x; ++x)
                for (int y = 0; y < avSubDiv.y; ++y) {
                    int lIdx = y * avSubDiv.x + x;

                    float fX = (float)x;
                    float fY = (float)y;

                    cPESubDivision* pSubDiv = &mvSubDivUV[lIdx];

                    pSubDiv->mvUV[0] = cVector3f((fX + 1) * fInvW, (fY + 1) * fInvH, 0); // 1,1
                    pSubDiv->mvUV[1] = cVector3f(fX * fInvW, (fY + 1) * fInvH, 0); // 0,1
                    pSubDiv->mvUV[2] = cVector3f(fX * fInvW, fY * fInvH, 0); // 0,0
                    pSubDiv->mvUV[3] = cVector3f((fX + 1) * fInvW, fY * fInvH, 0); // 1,0
                }
        }
    }

    //-----------------------------------------------------------------------

    // Seems like this fucntion is never called any more...
    void iParticleEmitter::UpdateLogic(float afTimeStep) {
        if (IsActive() == false)
            return;

        //////////////////////////////
        // Update sleep
        if (IsDying() == false && iRenderer::GetRenderFrameCount() != mlRenderFrameCount) {
            if (mlSleepCount <= 0)
                return;
            mlSleepCount--;
        } else {
            if (mlSleepCount < 10)
                mlSleepCount = 10;
        }

        //////////////////////////////
        // Update vars
        mbUpdateGfx = true;
        mbUpdateBV = true;

        //////////////////////////////
        // Update direction
        if (mbUsesDirection) {
            if (mlDirectionUpdateCount != GetMatrixUpdateCount()) {
                mlDirectionUpdateCount = GetMatrixUpdateCount();
                cMatrixf mtxInv = cMath::MatrixInverse(GetWorldMatrix());
                mvDirection = mtxInv.GetUp();
            }
        }

        UpdateMotion(afTimeStep);

        SetTransformUpdated();
    }

    void iParticleEmitter::KillInstantly() {
        mlMaxParticles = 0;
        mlNumOfParticles = 0;
        mbDying = true;
    }

    cMaterial* iParticleEmitter::GetMaterial() {
        return (*mvMaterials)[(int)mfFrame];
    }

    bool iParticleEmitter::UpdateGraphicsForViewport(cFrustum* apFrustum, float afFrameTime) {
        if(mlMaxParticles == 0) {
            return true;
        }
        ASSERT(mlNumOfParticles <= mlMaxParticles);


        m_activeCopy = (m_activeCopy + 1) % NumberActiveCopies;

        auto positionStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_POSITION);
        auto colorStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_COLOR);
        auto uvStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_TEXCOORD0);


        BufferUpdateDesc positionUpdateDesc = { positionStream->buffer().m_handle,
                                                positionStream->stride() * ((mlMaxParticles * m_activeCopy * 4) + m_geometry->vertexOffset()),
                                                positionStream->stride() * mlNumOfParticles * 4 };
        BufferUpdateDesc textureUpdateDesc = { uvStream->buffer().m_handle,
                                               uvStream->stride() * ((mlMaxParticles * m_activeCopy * 4) + m_geometry->vertexOffset()),
                                               uvStream->stride() * mlNumOfParticles * 4 };
        BufferUpdateDesc colorUpdateDesc = { colorStream->buffer().m_handle,
                                             colorStream->stride() * ((mlMaxParticles * m_activeCopy * 4) + m_geometry->vertexOffset()),
                                             colorStream->stride() * mlNumOfParticles * 4 };

        // Set up color mul
        cColor colorMul = mpParentSystem->mColor;

        // Set alpha based on fade distance.
        if (mpParentSystem->mbFadeAtDistance) {
            float fDistSqr = cMath::Vector3DistSqr(mpParentSystem->GetBoundingVolume()->GetWorldCenter(), apFrustum->GetOrigin());
            float fMinStart = mpParentSystem->mfMinFadeDistanceStart;
            float fMaxStart = mpParentSystem->mfMaxFadeDistanceStart;
            float fMinEnd = mpParentSystem->mfMinFadeDistanceEnd;
            float fMaxEnd = mpParentSystem->mfMaxFadeDistanceEnd;

            // Below min
            if (fMinStart > 0 && fDistSqr < fMinStart * fMinStart) {
                if (fDistSqr <= fMinEnd * fMinEnd) {
                    colorMul.a = 0;
                } else {
                    float fDist = sqrt(fDistSqr);
                    colorMul.a *= (fDist - fMinEnd) / (fMinStart - fMinEnd);
                }
            }
            // Above max
            if (fMaxStart > 0 && fDistSqr > fMaxStart * fMaxStart) {
                if (fDistSqr >= fMaxEnd * fMaxEnd) {
                    colorMul.a = 0;
                } else {
                    float fDist = sqrt(fDistSqr);
                    colorMul.a *= 1 - (fDist - fMaxStart) / (fMaxEnd - fMaxStart);
                }
            }
        }
        // Change color based on alpha
        if (mbMultiplyRGBWithAlpha) {
            colorMul.r *= colorMul.a;
            colorMul.g *= colorMul.a;
            colorMul.b *= colorMul.a;
        }

        // If alpha is 0, skip rendering anything
        m_numberParticlesRender = mlNumOfParticles;
        if (colorMul.a <= 0) {
            m_numberParticlesRender = 0;
            return false;
        }

        if (mPEType == ePEType_Beam) {
            // something something beam Idunno
        } else {

            if (mvSubDivUV.size() > 1) {
                beginUpdateResource(&textureUpdateDesc);
                GraphicsBuffer gpuUvBuffer(textureUpdateDesc);
                auto textureView = gpuUvBuffer.CreateStructuredView<float2>();

                for (uint32_t i = 0; i < mlNumOfParticles; i++) {
                    cParticle* pParticle = mvParticles[i];

                    cPESubDivision& subDiv = mvSubDivUV[pParticle->mlSubDivNum];
                    textureView.Write((i * 4) + 0, float2(subDiv.mvUV[0].x, subDiv.mvUV[0].y));
                    textureView.Write((i * 4) + 1, float2(subDiv.mvUV[1].x, subDiv.mvUV[1].y));
                    textureView.Write((i * 4) + 2, float2(subDiv.mvUV[2].x, subDiv.mvUV[2].y));
                    textureView.Write((i * 4) + 3, float2(subDiv.mvUV[3].x, subDiv.mvUV[3].y));
                }
                endUpdateResource(&textureUpdateDesc);
            }

            // FIXED POINT
            switch (mDrawType) {
            case eParticleEmitterType_FixedPoint:
                {
                    beginUpdateResource(&positionUpdateDesc);
                    beginUpdateResource(&colorUpdateDesc);
                    GraphicsBuffer gpuPositionBuffer(positionUpdateDesc);
                    GraphicsBuffer gpuColorBuffer(colorUpdateDesc);
                    auto positionView = gpuPositionBuffer.CreateStructuredView<float3>();
                    auto colorView = gpuColorBuffer.CreateStructuredView<float4>();

                    cVector3f vAdd[4] = { cVector3f(mvDrawSize.x, -mvDrawSize.y, 0),
                                          cVector3f(-mvDrawSize.x, -mvDrawSize.y, 0),
                                          cVector3f(-mvDrawSize.x, mvDrawSize.y, 0),
                                          cVector3f(mvDrawSize.x, mvDrawSize.y, 0) };

                    // If this is a reflection, need to invert the ordering.
                    if (apFrustum->GetInvertsCullMode()) {
                        for (int i = 0; i < 4; ++i)
                            vAdd[i].y = -vAdd[i].y;
                    }

                    for (uint32_t i = 0; i < mlNumOfParticles; i++) {
                        cParticle* pParticle = mvParticles[i];

                        // This is not the fastest thing possible...
                        cVector3f vParticlePos = pParticle->mvPos;

                        if (mCoordSystem == eParticleEmitterCoordSystem_Local) {
                            vParticlePos = cMath::MatrixMul(mpParentSystem->GetWorldMatrix(), vParticlePos);
                        }

                        cVector3f vPos = cMath::MatrixMul(apFrustum->GetViewMatrix(), vParticlePos);
                        cColor finalColor = pParticle->mColor * colorMul;

                        positionView.Write((i * 4) + 0, v3ToF3(cMath::ToForgeVec3(vPos + vAdd[0])));
                        positionView.Write((i * 4) + 1, v3ToF3(cMath::ToForgeVec3(vPos + vAdd[1])));
                        positionView.Write((i * 4) + 2, v3ToF3(cMath::ToForgeVec3(vPos + vAdd[2])));
                        positionView.Write((i * 4) + 3, v3ToF3(cMath::ToForgeVec3(vPos + vAdd[3])));

                        colorView.Write((i * 4) + 0, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                        colorView.Write((i * 4) + 1, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                        colorView.Write((i * 4) + 2, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                        colorView.Write((i * 4) + 3, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                    }

                    endUpdateResource(&positionUpdateDesc);
                    endUpdateResource(&colorUpdateDesc);
                    break;
                }
            case eParticleEmitterType_DynamicPoint:
                {
                    beginUpdateResource(&positionUpdateDesc);
                    beginUpdateResource(&colorUpdateDesc);
                    GraphicsBuffer gpuPositionBuffer(positionUpdateDesc);
                    GraphicsBuffer gpuColorBuffer(colorUpdateDesc);
                    auto positionView = gpuPositionBuffer.CreateStructuredView<float3>();
                    auto colorView = gpuColorBuffer.CreateStructuredView<float4>();

                    cVector3f vAdd[4] = { cVector3f(mvDrawSize.x, -mvDrawSize.y, 0),
                                          cVector3f(-mvDrawSize.x, -mvDrawSize.y, 0),
                                          cVector3f(-mvDrawSize.x, mvDrawSize.y, 0),
                                          cVector3f(mvDrawSize.x, mvDrawSize.y, 0) };

                    // If this is a reflection, need to invert the ordering.
                    if (apFrustum->GetInvertsCullMode()) {
                        for (int i = 0; i < 4; ++i)
                            vAdd[i].y = -vAdd[i].y;
                    }

                    for (uint32_t i = 0; i < mlNumOfParticles; i++) {
                        cParticle* pParticle = mvParticles[i];

                        // This is not the fastest thing possible
                        cVector3f vParticlePos = pParticle->mvPos;

                        if (mCoordSystem == eParticleEmitterCoordSystem_Local) {
                            vParticlePos = cMath::MatrixMul(mpParentSystem->GetWorldMatrix(), vParticlePos);
                        }

                        cVector3f vPos = cMath::MatrixMul(apFrustum->GetViewMatrix(), vParticlePos);

                        cVector3f vParticleSize = pParticle->mvSize;
                        cColor finalColor = pParticle->mColor * colorMul;

                        if (mbUsePartSpin) {
                            cMatrixf mtxRotationMatrix = cMath::MatrixRotateZ(pParticle->mfSpin);

                            positionView.Write(
                                (i * 4) + 0,
                                v3ToF3(cMath::ToForgeVec3(vPos + cMath::MatrixMul(mtxRotationMatrix, vAdd[0] * vParticleSize))));
                            positionView.Write(
                                (i * 4) + 1,
                                v3ToF3(cMath::ToForgeVec3(vPos + cMath::MatrixMul(mtxRotationMatrix, vAdd[1] * vParticleSize))));
                            positionView.Write(
                                (i * 4) + 2,
                                v3ToF3(cMath::ToForgeVec3(vPos + cMath::MatrixMul(mtxRotationMatrix, vAdd[2] * vParticleSize))));
                            positionView.Write(
                                (i * 4) + 3,
                                v3ToF3(cMath::ToForgeVec3(vPos + cMath::MatrixMul(mtxRotationMatrix, vAdd[3] * vParticleSize))));

                            colorView.Write((i * 4) + 0, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                            colorView.Write((i * 4) + 1, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                            colorView.Write((i * 4) + 2, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                            colorView.Write((i * 4) + 3, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));

                        } else {
                            positionView.Write((i * 4) + 0, v3ToF3(cMath::ToForgeVec3(vPos + vAdd[0] * vParticleSize)));
                            positionView.Write((i * 4) + 1, v3ToF3(cMath::ToForgeVec3(vPos + vAdd[1] * vParticleSize)));
                            positionView.Write((i * 4) + 2, v3ToF3(cMath::ToForgeVec3(vPos + vAdd[2] * vParticleSize)));
                            positionView.Write((i * 4) + 3, v3ToF3(cMath::ToForgeVec3(vPos + vAdd[3] * vParticleSize)));

                            colorView.Write((i * 4) + 0, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                            colorView.Write((i * 4) + 1, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                            colorView.Write((i * 4) + 2, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                            colorView.Write((i * 4) + 3, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                        }
                    }

                    endUpdateResource(&positionUpdateDesc);
                    endUpdateResource(&colorUpdateDesc);
                    break;
                }
            case eParticleEmitterType_Line:
                {
                    beginUpdateResource(&positionUpdateDesc);
                    beginUpdateResource(&colorUpdateDesc);
                    GraphicsBuffer gpuPositionBuffer(positionUpdateDesc);
                    GraphicsBuffer gpuColorBuffer(colorUpdateDesc);
                    auto positionView = gpuPositionBuffer.CreateStructuredView<float3>();
                    auto colorView = gpuColorBuffer.CreateStructuredView<float4>();

                    for (uint32_t i = 0; i < mlNumOfParticles; i++) {
                        cParticle* pParticle = mvParticles[i];

                        // This is not the fastest thing possible...

                        cVector3f vParticlePos1 = pParticle->mvPos;
                        cVector3f vParticlePos2 = pParticle->mvLastPos;

                        if (mCoordSystem == eParticleEmitterCoordSystem_Local) {
                            vParticlePos1 = cMath::MatrixMul(mpParentSystem->GetWorldMatrix(), vParticlePos1);
                            vParticlePos2 = cMath::MatrixMul(mpParentSystem->GetWorldMatrix(), vParticlePos2);
                        }

                        cVector3f vPos1 = cMath::MatrixMul(apFrustum->GetViewMatrix(), vParticlePos1);
                        cVector3f vPos2 = cMath::MatrixMul(apFrustum->GetViewMatrix(), vParticlePos2);

                        cVector3f vDirY;
                        cVector2f vDirX;

                        if (vPos1 == vPos2) {
                            vDirY = cVector3f(0, 1, 0);
                            vDirX = cVector2f(1, 0);
                        } else {
                            vDirY = vPos1 - vPos2;
                            vDirY.Normalize();
                            vDirX = cVector2f(vDirY.y, -vDirY.x);
                            vDirX.Normalize();
                        }

                        vDirX = vDirX * mvDrawSize.x * pParticle->mvSize.x;
                        vDirY = vDirY * mvDrawSize.y * pParticle->mvSize.y;

                        if (apFrustum->GetInvertsCullMode())
                            vDirY = vDirY * -1;

                        cColor finalColor = pParticle->mColor * colorMul;

                        positionView.Write((i * 4) + 0, v3ToF3(cMath::ToForgeVec3(vPos2 + vDirY * -1 + vDirX)));
                        positionView.Write((i * 4) + 1, v3ToF3(cMath::ToForgeVec3(vPos2 + vDirY * -1 + vDirX * -1)));
                        positionView.Write((i * 4) + 2, v3ToF3(cMath::ToForgeVec3(vPos1 + vDirY + vDirX * -1)));
                        positionView.Write((i * 4) + 3, v3ToF3(cMath::ToForgeVec3(vPos1 + vDirY + vDirX)));

                        colorView.Write((i * 4) + 0, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                        colorView.Write((i * 4) + 1, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                        colorView.Write((i * 4) + 2, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                        colorView.Write((i * 4) + 3, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                    }

                    endUpdateResource(&positionUpdateDesc);
                    endUpdateResource(&colorUpdateDesc);

                    break;
                }
            case eParticleEmitterType_Axis: {
                    beginUpdateResource(&positionUpdateDesc);
                    beginUpdateResource(&colorUpdateDesc);
                    GraphicsBuffer gpuPositionBuffer(positionUpdateDesc);
                    GraphicsBuffer gpuColorBuffer(colorUpdateDesc);
                    auto positionView = gpuPositionBuffer.CreateStructuredView<float3>();
                    auto colorView = gpuColorBuffer.CreateStructuredView<float4>();

                    if (mlAxisDrawUpdateCount != GetMatrixUpdateCount()) {
                        mlAxisDrawUpdateCount = GetMatrixUpdateCount();
                        cMatrixf mtxInv = cMath::MatrixInverse(GetWorldMatrix());
                        mvRight = mtxInv.GetRight();
                        mvForward = mtxInv.GetForward();
                    }

                    cVector3f vAdd[4];

                    for (int i = 0; i < (int)mlNumOfParticles; i++) {
                        cParticle* pParticle = mvParticles[i];

                        // This is not the fastest thing possible
                        cVector3f vParticlePos = pParticle->mvPos;

                        if (mCoordSystem == eParticleEmitterCoordSystem_Local) {
                            vParticlePos = cMath::MatrixMul(mpParentSystem->GetWorldMatrix(), vParticlePos);
                        }

                        cVector3f vPos = vParticlePos; // cMath::MatrixMul(apCamera->GetViewMatrix(), vParticlePos);
                        cVector2f& vSize = pParticle->mvSize;

                        vAdd[0] = mvRight * vSize.x + mvForward * vSize.y;
                        vAdd[1] = mvRight * -vSize.x + mvForward * vSize.y;
                        vAdd[2] = mvRight * -vSize.x + mvForward * -vSize.y;
                        vAdd[3] = mvRight * vSize.x + mvForward * -vSize.y;

                        cColor finalColor = pParticle->mColor * colorMul;

                        positionView.Write((i * 4) + 0, v3ToF3(cMath::ToForgeVec3(vPos + vAdd[0])));
                        positionView.Write((i * 4) + 1, v3ToF3(cMath::ToForgeVec3(vPos + vAdd[1])));
                        positionView.Write((i * 4) + 2, v3ToF3(cMath::ToForgeVec3(vPos + vAdd[2])));
                        positionView.Write((i * 4) + 3, v3ToF3(cMath::ToForgeVec3(vPos + vAdd[3])));

                        colorView.Write((i * 4) + 0, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                        colorView.Write((i * 4) + 1, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                        colorView.Write((i * 4) + 2, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                        colorView.Write((i * 4) + 3, float4(finalColor.r, finalColor.g, finalColor.b, finalColor.a));
                    }

                    endUpdateResource(&positionUpdateDesc);
                    endUpdateResource(&colorUpdateDesc);
                    break;
                }
            }

        }

        return true;
    }

    DrawPacket iParticleEmitter::ResolveDrawPacket(const ForgeRenderer::Frame& frame) {
        DrawPacket packet;
        if (m_numberParticlesRender == 0) {
            return packet;
        }

        DrawPacket::GeometrySetBinding binding{};
        packet.m_type = DrawPacket::DrawGeometryset;
        binding.m_subAllocation = m_geometry.get();
        binding.m_set = GraphicsAllocator::AllocationSet::ParticleSet;
        binding.m_indexOffset = 0;
        binding.m_numIndices = m_numberParticlesRender * 6;
        binding.m_vertexOffset = (mlMaxParticles * m_activeCopy * 4);
        packet.m_unified = binding;
        return packet;
    }

    iVertexBuffer* iParticleEmitter::GetVertexBuffer() {
        return nullptr;
    }

    bool iParticleEmitter::IsVisible() {
        if (IsActive() == false)
            return false;
        return mbIsVisible;
    }

    cBoundingVolume* iParticleEmitter::GetBoundingVolume() {
        if (mbUpdateBV) {
            cVector3f vMin;
            cVector3f vMax;

            if (mlNumOfParticles > 0) {
                // Make a bounding volume that encompasses start pos too!
                vMin = GetWorldPosition();
                vMax = GetWorldPosition();

                for (int i = 0; i < (int)mlNumOfParticles; i++) {
                    cParticle* pParticle = mvParticles[i];

                    // X
                    if (pParticle->mvPos.x < vMin.x)
                        vMin.x = pParticle->mvPos.x;
                    else if (pParticle->mvPos.x > vMax.x)
                        vMax.x = pParticle->mvPos.x;

                    // Y
                    if (pParticle->mvPos.y < vMin.y)
                        vMin.y = pParticle->mvPos.y;
                    else if (pParticle->mvPos.y > vMax.y)
                        vMax.y = pParticle->mvPos.y;

                    // Z
                    if (pParticle->mvPos.z < vMin.z)
                        vMin.z = pParticle->mvPos.z;
                    else if (pParticle->mvPos.z > vMax.z)
                        vMax.z = pParticle->mvPos.z;
                }
            } else {
                vMin = GetWorldPosition();
                vMax = GetWorldPosition();
                ;
            }

            /////////////////////////7
            // Add size for axis
            if (mDrawType == eParticleEmitterType_Axis) {
                if (mlAxisDrawUpdateCount != GetMatrixUpdateCount()) {
                    mlAxisDrawUpdateCount = GetMatrixUpdateCount();
                    cMatrixf mtxInv = cMath::MatrixInverse(GetWorldMatrix());
                    mvRight = mtxInv.GetRight();
                    mvForward = mtxInv.GetForward();
                }

                cVector3f vAdd = mvRight * mvMaxDrawSize.x + mvForward * mvMaxDrawSize.y;

                vMax += vAdd;
                vMin -= vAdd;
            }
            /////////////////////////7
            // Add size for other particle types.
            else {
                vMax += cVector3f(mvMaxDrawSize.x, mvMaxDrawSize.y, mvMaxDrawSize.x);
                vMin -= cVector3f(mvMaxDrawSize.x, mvMaxDrawSize.y, mvMaxDrawSize.x);
            }

            mBoundingVolume.SetLocalMinMax(vMin, vMax);

            // Log("Min: (%f, %f, %f) Max: (%f, %f, %f)\n", vMin.x, vMin.y, vMin.z, vMax.x,vMax.y,vMax.z);

            if (mCoordSystem == eParticleEmitterCoordSystem_Local) {
                mBoundingVolume.SetTransform(mpParentSystem->GetWorldMatrix());
            }

            mbUpdateBV = false;
        }

        return &mBoundingVolume;
    }

    cMatrixf* iParticleEmitter::GetModelMatrix(cFrustum* apFrustum) {
        if (apFrustum) {
            if (mDrawType == eParticleEmitterType_Axis) {
                m_mtxTemp = cMatrixf::Identity;
            }
            // This is really not good...
            else if (mCoordSystem == eParticleEmitterCoordSystem_World) {
                m_mtxTemp = cMath::MatrixInverse(apFrustum->GetViewMatrix());
            } else {
                m_mtxTemp = cMath::MatrixInverse(apFrustum->GetViewMatrix());
                // m_mtxTemp = cMath::MatrixMul(cMath::MatrixInverse(apCamera->GetViewMatrix()),
                //								GetWorldMatrix());
            }

            // m_mtxTemp.SetTranslation(cVector3f(0,0,0));//GetWorldMatrix().GetTranslation());

            // m_mtxTemp = cMatrixf::Identity;

            // cMatrixf mtxCam = apCamera->GetViewMatrix();
            // Log("MATRIX: %s\n",mtxCam.ToString().c_str());

            return &m_mtxTemp;

        } else {
            return &GetWorldMatrix();
        }
    }

    cParticle* iParticleEmitter::CreateParticle() {
        if (mlNumOfParticles == mlMaxParticles)
            return NULL;
        ++mlNumOfParticles;
        return mvParticles[mlNumOfParticles - 1];
    }

    void iParticleEmitter::SwapRemove(unsigned int alIndex) {
        if (alIndex < mlNumOfParticles - 1) {
            cParticle* pTemp = mvParticles[alIndex];
            mvParticles[alIndex] = mvParticles[mlNumOfParticles - 1];
            mvParticles[mlNumOfParticles - 1] = pTemp;
        }
        mlNumOfParticles--;
    }

} // namespace hpl
