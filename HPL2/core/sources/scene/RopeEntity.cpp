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

#include "scene/RopeEntity.h"

#include "bgfx/bgfx.h"
#include "graphics/BufferIndex.h"
#include "graphics/BufferVertex.h"
#include "math/Math.h"

#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Material.h"
#include "graphics/VertexBuffer.h"

#include "math/MathTypes.h"
#include "resources/MaterialManager.h"
#include "resources/Resources.h"

#include "scene/Camera.h"
#include "scene/Scene.h"
#include "scene/World.h"

#include "physics/PhysicsRope.h"
#include <algorithm>

namespace hpl
{

    static const BufferVertex::VertexDefinition VertexDefinition = 
		([]() {
			BufferVertex::VertexDefinition definition;
			definition.m_layout.begin()
				.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.end();
            definition.m_accessType = BufferVertex::AccessType::AccessDynamic;
			return definition;
		})();
    static const BufferIndex::BufferIndexDefinition IndexDefinition = ([]() {
		BufferIndex::BufferIndexDefinition definition;
		definition.m_format = BufferIndex::IndexFormatType::Uint16;
        
		return definition;
	})();

    struct RopeVertexElement
    {
        float position[3];
        float normal[3];
        float tangent[4];
        float color[4];
        float texcoord[2];
    };

    cRopeEntity::cRopeEntity(const tString& asName, cResources* apResources, cGraphics* apGraphics, iPhysicsRope* apRope, int alMaxSegments)
        : iRenderable(asName)
    {
        mpMaterialManager = apResources->GetMaterialManager();
        mpLowLevelGraphics = apGraphics->GetLowLevel();

        mColor = cColor(1, 1, 1, 1);

        mpMaterial = NULL;

        mpRope = apRope;
        mlMaxSegments = alMaxSegments;

        mfRadius = mpRope->GetParticleRadius();
        mfLengthTileAmount = 1;
        mfLengthTileSize = 1;

        BufferVertex vtxData(VertexDefinition);
        BufferIndex indexData(IndexDefinition);

        for (int i = 0; i < mlMaxSegments; ++i)
        {
            const cVector2f vTexCoords[4] = { cVector2f(1, 1), // Bottom left
                                              cVector2f(-1, 1), // Bottom right
                                              cVector2f(-1, -1), // Top left
                                              cVector2f(1, -1) }; // Top right

            for (int j = 0; j < 4; j++)
            {
                vtxData.Append<RopeVertexElement>({ { 0, 0, 0 },
                                                       { 0, 0, 1 },
                                                       { 0, 0, 0 },
                                                       { mColor.r, mColor.g, mColor.b, mColor.a },
                                                       { vTexCoords[j].x, vTexCoords[j].y } });
                for (int j = 0; j < 3; j++)
                {
                    indexData.Append(j + i * 4);
                }
                for (int j = 2; j < 5; j++)
                {
                    indexData.Append((j == 4 ? 0 : j) + i * 4);
                }
            }
        }
        BufferVertexView(&vtxData)
            .UpdateTangentsFromPositionTexCoords(BufferIndexView(&indexData));
        
        vtxData.Initialize();
        indexData.Initialize();

        mbApplyTransformToBV = false;
        mlLastUpdateCount = -1;

        m_vtxData = std::move(vtxData);
        m_idxData = std::move(indexData);
    }

    //-----------------------------------------------------------------------

    cRopeEntity::~cRopeEntity()
    {
        if (mpMaterial)
            mpMaterialManager->Destroy(mpMaterial);
        if (mpVtxBuffer)
            hplDelete(mpVtxBuffer);
    }

    void cRopeEntity::SetMultiplyAlphaWithColor(bool abX)
    {
        if (mbMultiplyAlphaWithColor == abX)
            return;

        mbMultiplyAlphaWithColor = abX;
    }

    void cRopeEntity::SetColor(const cColor& aColor)
    {
        if (mColor == aColor)
            return;

        mColor = aColor;
        cColor finalColor = mColor;
        if (mbMultiplyAlphaWithColor)
        {
            finalColor.r = finalColor.r * mColor.a;
            finalColor.g = finalColor.g * mColor.a;
            finalColor.b = finalColor.b * mColor.a;
        }

        for (auto& element : m_vtxData.GetElements<RopeVertexElement>())
        {
            element.color[0] = finalColor.r;
            element.color[1] = finalColor.g;
            element.color[2] = finalColor.b;
            element.color[3] = finalColor.a;
        }
        mpVtxBuffer->UpdateData(eVertexElementFlag_Color0, false);
    }

    //-----------------------------------------------------------------------

    void cRopeEntity::SetMaterial(cMaterial* apMaterial)
    {
        mpMaterial = apMaterial;
    }

    //-----------------------------------------------------------------------

    cBoundingVolume* cRopeEntity::GetBoundingVolume()
    {
        if (mlLastUpdateCount != mpRope->GetUpdateCount())
        {
            cVector3f vMin(1000000.0f);
            cVector3f vMax(-1000000.0f);

            cVerletParticleIterator it = mpRope->GetParticleIterator();
            while (it.HasNext())
            {
                cVerletParticle* pPart = it.Next();
                cMath::ExpandAABB(vMin, vMax, pPart->GetPosition(), pPart->GetPosition());
            }

            mBoundingVolume.SetLocalMinMax(vMin - cVector3f(mfRadius), vMax + cVector3f(mfRadius));

            mlLastUpdateCount = mpRope->GetUpdateCount();
        }

        return &mBoundingVolume;
    }

    //-----------------------------------------------------------------------

    void cRopeEntity::UpdateGraphicsForFrame(float afFrameTime)
    {
    }

    static cVector2f gvPosAdd[4] = {
        // cVector2f (1,1), cVector2f (1,0), cVector2f (-1,0), cVector2f (-1,1)
        cVector2f(1, 0),
        cVector2f(-1, 0),
        cVector2f(-1, 1),
        cVector2f(1, 1)
    };

    bool cRopeEntity::UpdateGraphicsForViewport(cFrustum* apFrustum, float afFrameTime)
    {
        auto elements = m_vtxData.GetElements<RopeVertexElement>();
        auto elementIt = elements.begin();

        float fSegmentLength = mpRope->GetSegmentLength();

        cVector3f vTexCoords[4] = {
            cVector3f(1, 1, 0), // Bottom left
            cVector3f(0, 1, 0), // Bottom right
            cVector3f(0, 0, 0), // Top left
            cVector3f(1, 0, 0) // Top right
        };

        vTexCoords[0].y *= mfLengthTileAmount;
        vTexCoords[1].y *= mfLengthTileAmount;

        cVerletParticleIterator it = mpRope->GetParticleIterator();
        int lCount = 0;
        cVector3f vPrevPos;
        while (it.HasNext())
        {
            if (lCount >= mlMaxSegments)
                break;
            ++lCount;

            cVerletParticle* pPart = it.Next();

            if (lCount == 1)
            {
                vPrevPos = pPart->GetPosition();
                continue;
            }

            /////////////////////////
            // Calculate properties
            cVector3f vPos = pPart->GetSmoothPosition();
            cVector3f vDelta = vPos - vPrevPos;
            float fLength = vDelta.Length();
            cVector3f vUp = vDelta / fLength;
            cVector3f vRight = cMath::Vector3Normalize(cMath::Vector3Cross(vUp, apFrustum->GetForward()));
            cVector3f vFwd = cMath::Vector3Cross(vRight, vUp);

            /////////////////////////
            // Update position
            for (int i = 0; i < 4; ++i)
            {
                cVector3f pos = vPrevPos + vRight * gvPosAdd[i].x * mfRadius + vUp * gvPosAdd[i].y * fLength;
                elementIt[i].position[0] = pos.x;
                elementIt[i].position[1] = pos.y;
                elementIt[i].position[2] = pos.z;
            }

            /////////////////////////
            // Update uv
            if (lCount == 2 && (fLength < fSegmentLength || fSegmentLength == 0))
            {
                //////////////////
                // No segments
                if (fSegmentLength == 0)
                {
                    float fYAdd = 1 - fLength / mfLengthTileSize;
                    const cVector3f uv1 = vTexCoords[0] - cVector3f(0, fYAdd, 0);
                    const cVector3f uv2 = vTexCoords[1] - cVector3f(0, fYAdd, 0);
                    const cVector3f uv3 = vTexCoords[2];
                    const cVector3f uv4 = vTexCoords[3];

                    std::copy(uv1.v, uv1.v + 2, elementIt[0 * 3].texcoord);
                    std::copy(uv2.v, uv2.v + 2, elementIt[1 * 3].texcoord);
                    std::copy(uv3.v, uv3.v + 2, elementIt[2 * 3].texcoord);
                    std::copy(uv4.v, uv4.v + 2, elementIt[3 * 3].texcoord);
                }
                //////////////////
                // First segment of many
                else
                {
                    float fYAdd = (1 - (fLength / fSegmentLength)) * mfLengthTileAmount;
                    const cVector3f uv1 = vTexCoords[0] - cVector3f(0, fYAdd, 0);
                    const cVector3f uv2 = vTexCoords[1] - cVector3f(0, fYAdd, 0);
                    const cVector3f uv3 = vTexCoords[2];
                    const cVector3f uv4 = vTexCoords[3];

                    std::copy(uv1.v, uv1.v + 2, elementIt[0].texcoord);
                    std::copy(uv2.v, uv2.v + 2, elementIt[1].texcoord);
                    std::copy(uv3.v, uv3.v + 2, elementIt[2].texcoord);
                    std::copy(uv4.v, uv4.v + 2, elementIt[3].texcoord);
                }
            }
            else
            {
                for (int i = 0; i < 4; ++i)
                {
                    const cVector3f uv = vTexCoords[i];
                    std::copy(uv.v, uv.v + 2, elementIt[i].texcoord);
                }
            }

            /////////////////////////
            // Update Normal and Tangent
            for (int i = 0; i < 4; ++i)
            {
                std::copy(vFwd.v, vFwd.v + 3, elementIt[i].normal);
                std::copy(vRight.v, vRight.v + 3, elementIt[i].tangent);
            }

            elementIt += 4;
            vPrevPos = vPos;
        }

        // mpVtxBuffer->SetElementNum((lCount - 1) * 6);
        m_vtxData.Update(0, (lCount - 1) * 4);

        // mpVtxBuffer->UpdateData(
        //     eVertexElementFlag_Position | eVertexElementFlag_Texture0 | eVertexElementFlag_Texture1 | eVertexElementFlag_Normal, false);

        return true;
    }

    //-----------------------------------------------------------------------

    cMatrixf* cRopeEntity::GetModelMatrix(cFrustum* apFrustum)
    {
        if (apFrustum == NULL)
            return &GetWorldMatrix();

        return NULL;
    }

    //-----------------------------------------------------------------------

    int cRopeEntity::GetMatrixUpdateCount()
    {
        return GetTransformUpdateCount();
    }

    //-----------------------------------------------------------------------

    bool cRopeEntity::IsVisible()
    {
        if (mColor.r <= 0 && mColor.g <= 0 && mColor.b <= 0)
            return false;

        return mbIsVisible;
    }
} // namespace hpl
