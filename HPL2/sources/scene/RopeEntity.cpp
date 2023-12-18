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

#include "impl/LegacyVertexBuffer.h"
#include "math/Math.h"

#include "graphics/Graphics.h"
#include "graphics/Material.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/VertexBuffer.h"
#include <graphics/GraphicsBuffer.h>
#include <graphics/MeshUtility.h>

#include "resources/Resources.h"
#include "resources/MaterialManager.h"

#include "scene/Camera.h"
#include "scene/World.h"
#include "scene/Scene.h"

#include "physics/PhysicsRope.h"

namespace hpl {

	cRopeEntity::cRopeEntity(const tString& asName, cResources *apResources,cGraphics *apGraphics,
								iPhysicsRope *apRope, int alMaxSegments) :	iRenderable(asName)
	{
		mpMaterialManager = apResources->GetMaterialManager();
		mpLowLevelGraphics = apGraphics->GetLowLevel();

		mColor = cColor(1,1,1,1);

		mpMaterial = NULL;

		mpRope = apRope;
		mlMaxSegments = alMaxSegments;

		mfRadius = mpRope->GetParticleRadius();
		mfLengthTileAmount = 1;
		mfLengthTileSize = 1;

        {
            auto* graphicsAllocator = Interface<GraphicsAllocator>::Get();
            auto& opaqueSet = graphicsAllocator->resolveSet(GraphicsAllocator::AllocationSet::OpaqueSet);
            m_geometry = opaqueSet.allocate(4 * mlMaxSegments, 6 * mlMaxSegments);

            auto& indexStream = m_geometry->indexBuffer();
            auto positionStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_POSITION);
            auto normalStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_NORMAL);
            auto colorStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_COLOR);
            auto textureStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_TEXCOORD0);
            auto tangentStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_TANGENT);

            BufferUpdateDesc indexUpdateDesc = { indexStream.m_handle,
                                                 0,
                                                 GeometrySet::IndexBufferStride * 6 * mlMaxSegments * NumberOfCopies };
            BufferUpdateDesc positionUpdateDesc = { positionStream->buffer().m_handle,
                                                    0,
                                                    positionStream->stride() * 4 * mlMaxSegments * NumberOfCopies };
            BufferUpdateDesc normalUpdateDesc = { normalStream->buffer().m_handle,
                                                  0,
                                                  normalStream->stride() * 4 * mlMaxSegments * NumberOfCopies };
            BufferUpdateDesc colorUpdateDesc = { colorStream->buffer().m_handle,
                                                 0,
                                                 colorStream->stride() * 4 * mlMaxSegments * NumberOfCopies };
            BufferUpdateDesc textureUpdateDesc = { textureStream->buffer().m_handle,
                                                   0,
                                                   textureStream->stride() * 4 * mlMaxSegments * NumberOfCopies };
            BufferUpdateDesc tangentUpdateDesc = { tangentStream->buffer().m_handle,
                                                   0,
                                                   tangentStream->stride() * 4 * mlMaxSegments * NumberOfCopies };

            beginUpdateResource(&indexUpdateDesc);
            beginUpdateResource(&positionUpdateDesc);
            beginUpdateResource(&normalUpdateDesc);
            beginUpdateResource(&colorUpdateDesc);
            beginUpdateResource(&textureUpdateDesc);
            beginUpdateResource(&tangentUpdateDesc);

            GraphicsBuffer gpuIndexBuffer(indexUpdateDesc);
            GraphicsBuffer gpuPositionBuffer(positionUpdateDesc);
            GraphicsBuffer gpuNormalBuffer(normalUpdateDesc);
            GraphicsBuffer gpuColorBuffer(colorUpdateDesc);
            GraphicsBuffer gpuTextureBuffer(textureUpdateDesc);
            GraphicsBuffer gpuTangentBuffer(tangentUpdateDesc);
            auto indexView = gpuPositionBuffer.CreateIndexView();
		    uint32_t index = 0;
            for (int i = 0; i < mlMaxSegments; ++i) {
                for (int j = 0; j < 3; j++) {
                    indexView.Write(index, j + i * 4);
                }
                for (int j = 2; j < 5; j++) {
                    indexView.Write(index, (j == 4 ? 0 : j) + i * 4);
                }
            }

            for(size_t copyIdx = 0; copyIdx < 2; copyIdx++) {
                auto positionView = gpuPositionBuffer.CreateStructuredView<float3>(positionStream->stride() * copyIdx * mlMaxSegments * 4);
                auto normalView = gpuNormalBuffer.CreateStructuredView<float3>(normalStream->stride() * copyIdx * mlMaxSegments * 4);
                auto colorView = gpuColorBuffer.CreateStructuredView<float4>(colorStream->stride() * copyIdx * mlMaxSegments * 4);
                auto textureView = gpuTextureBuffer.CreateStructuredView<float2>(textureStream->stride() * copyIdx * mlMaxSegments * 4);
                auto tangentView = gpuTangentBuffer.CreateStructuredView<float3>(tangentStream->stride() * copyIdx * mlMaxSegments * 4);
		        for(int i=0; i < mlMaxSegments; ++i)
		        {
		            for(size_t j = 0; j < 4; j++) {
                        tangentView.Write((i *  4) + j, float3(0, 0, 1));
                        positionView.Write((i *  4) + j, float3(0,0,0));
                        normalView.Write((i *  4) + j, float3(0.0f, 0.0f, 1.0f));
                        colorView.Write((i *  4) + j, float4(mColor.r, mColor.g, mColor.g, mColor.a));
                        textureView.Write((i *  4) + j, float2(0,0));
                    }
		        }
                Matrix4 trans = Matrix4::identity();
                hpl::MeshUtility::MikkTSpaceGenerate(4 * mlMaxSegments, 6 * mlMaxSegments, &indexView, &positionView, &textureView, &normalView, &tangentView);
		    }

            endUpdateResource(&indexUpdateDesc);
            endUpdateResource(&positionUpdateDesc);
            endUpdateResource(&normalUpdateDesc);
            endUpdateResource(&colorUpdateDesc);
            endUpdateResource(&textureUpdateDesc);
            endUpdateResource(&tangentUpdateDesc);
        }

		mbApplyTransformToBV = false;

		mlLastUpdateCount = -1;
	}

    DrawPacket cRopeEntity::ResolveDrawPacket(const ForgeRenderer::Frame& frame)  {
  		DrawPacket packet;
		if(m_numberSegments <= 0) {
            return packet;
		}

        DrawPacket::GeometrySetBinding binding{};
        packet.m_type = DrawPacket::DrawGeometryset;
        binding.m_subAllocation = m_geometry.get();
        binding.m_indexOffset = 0;
        binding.m_set = GraphicsAllocator::AllocationSet::OpaqueSet;
        binding.m_numIndices = 6 * m_numberSegments;
        binding.m_vertexOffset = (m_activeCopy * mlMaxSegments * 4);
        packet.m_unified = binding;
        return packet;
    }

    cRopeEntity::~cRopeEntity()
	{
		if(mpMaterial) mpMaterialManager->Destroy(mpMaterial);
	}


	void cRopeEntity::SetMultiplyAlphaWithColor(bool abX)
	{
		if(mbMultiplyAlphaWithColor == abX) return;

		mbMultiplyAlphaWithColor = abX;
	}

	//-----------------------------------------------------------------------

	void cRopeEntity::SetColor(const cColor &aColor)
	{
		if(mColor == aColor) return;

		mColor = aColor;

	}


	void cRopeEntity::SetMaterial(cMaterial * apMaterial)
	{
		mpMaterial = apMaterial;
	}


	cBoundingVolume* cRopeEntity::GetBoundingVolume()
	{
		if(mlLastUpdateCount != mpRope->GetUpdateCount())
		{
			cVector3f vMin(1000000.0f);
			cVector3f vMax(-1000000.0f);

			cVerletParticleIterator it = mpRope->GetParticleIterator();
			while(it.HasNext())
			{
				cVerletParticle *pPart = it.Next();

				cMath::ExpandAABB(vMin,vMax, pPart->GetPosition(), pPart->GetPosition());
			}

			mBoundingVolume.SetLocalMinMax(vMin-cVector3f(mfRadius),vMax+cVector3f(mfRadius));

			mlLastUpdateCount = mpRope->GetUpdateCount();
		}

		return &mBoundingVolume;
	}

	//-----------------------------------------------------------------------

	void cRopeEntity::UpdateGraphicsForFrame(float afFrameTime)
	{

	}

	static cVector2f gvPosAdd[4] = {
		//cVector2f (1,1), cVector2f (1,0), cVector2f (-1,0), cVector2f (-1,1)
		cVector2f (1,0), cVector2f (-1,0), cVector2f (-1,1), cVector2f (1,1)
	};

	bool cRopeEntity::UpdateGraphicsForViewport(cFrustum *apFrustum,float afFrameTime)
	{
        m_activeCopy = (m_activeCopy + 1) % NumberOfCopies;
		cColor finalColor = mColor;
		if(mbMultiplyAlphaWithColor)
		{
			finalColor.r = finalColor.r * mColor.a;
			finalColor.g = finalColor.g * mColor.a;
			finalColor.b = finalColor.b * mColor.a;
		}


        auto positionStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_POSITION);
        auto colorStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_COLOR);
        auto textureStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_TEXCOORD0);
        auto normalStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_NORMAL);
        auto tangentStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_TANGENT);

        BufferUpdateDesc positionUpdateDesc = { positionStream->buffer().m_handle, positionStream->stride()  * mlMaxSegments * 4 * m_activeCopy, positionStream->stride() * mlMaxSegments * 4 };
        BufferUpdateDesc textureUpdateDesc = { textureStream->buffer().m_handle, textureStream->stride()  * mlMaxSegments * 4 * m_activeCopy, textureStream->stride() * mlMaxSegments * 4 };
        BufferUpdateDesc colorUpdateDesc = { colorStream->buffer().m_handle, colorStream->stride()  * mlMaxSegments * 4 * m_activeCopy, colorStream->stride() * mlMaxSegments * 4 };
        BufferUpdateDesc normalUpdateDesc = { normalStream->buffer().m_handle, normalStream->stride()  * mlMaxSegments * 4 * m_activeCopy, normalStream->stride() * mlMaxSegments * 4 };
        BufferUpdateDesc tangentUpdateDesc = { tangentStream->buffer().m_handle, tangentStream->stride()  * mlMaxSegments * 4 * m_activeCopy, tangentStream->stride() * mlMaxSegments * 4 };

        beginUpdateResource(&positionUpdateDesc);
        beginUpdateResource(&textureUpdateDesc);
        beginUpdateResource(&colorUpdateDesc);
        beginUpdateResource(&normalUpdateDesc);
        beginUpdateResource(&tangentUpdateDesc);

        GraphicsBuffer gpuPositionBuffer(positionUpdateDesc);
        GraphicsBuffer gpuNormalBuffer(normalUpdateDesc);
        GraphicsBuffer gpuColorBuffer(colorUpdateDesc);
        GraphicsBuffer gpuTextureBuffer(textureUpdateDesc);
        GraphicsBuffer gpuTangentBuffer(tangentUpdateDesc);
        auto positionView = gpuPositionBuffer.CreateStructuredView<float3>();
        auto normalView = gpuNormalBuffer.CreateStructuredView<float3>();
        auto colorView = gpuColorBuffer.CreateStructuredView<float4>();
        auto textureView = gpuTextureBuffer.CreateStructuredView<float2>();
        auto tangentView = gpuTangentBuffer.CreateStructuredView<float3>();

		float fSegmentLength = mpRope->GetSegmentLength();

		cVector2f vTexCoords[4] = {
				cVector2f(1,1),	//Bottom left
				cVector2f(0,1),	//Bottom right
				cVector2f(0,0),	//Top left
				cVector2f(1,0)	//Top right
		};

		vTexCoords[0].y *= mfLengthTileAmount;
		vTexCoords[1].y *= mfLengthTileAmount;

		cVerletParticleIterator it = mpRope->GetParticleIterator();
		int lCount=0;
		cVector3f vPrevPos;

        size_t segmentIndex = 0;
	    while(it.HasNext())
		{
			if(lCount >= mlMaxSegments) break;
			++lCount;

			cVerletParticle *pPart = it.Next();

			if(lCount == 1){
				vPrevPos = pPart->GetPosition();
				continue;
			}

			//Calculate properties
			cVector3f vPos = pPart->GetSmoothPosition();
			cVector3f vDelta = vPos - vPrevPos;
			float fLength = vDelta.Length();
			cVector3f vUp = vDelta / fLength;
			cVector3f vRight = cMath::Vector3Normalize(cMath::Vector3Cross(vUp, apFrustum->GetForward()));
			cVector3f vFwd = cMath::Vector3Cross(vRight, vUp);

			//Update position
			for(int i=0; i<4; ++i) {
				positionView.Write(segmentIndex + i, v3ToF3(cMath::ToForgeVec3(vPrevPos + vRight * gvPosAdd[i].x*mfRadius + vUp * gvPosAdd[i].y*fLength)));
			    //SetVec4(&pPosArray[i*4], vPrevPos + vRight * gvPosAdd[i].x*mfRadius + vUp * gvPosAdd[i].y*fLength);
		    }

			//Update uv
			if(lCount==2 && (fLength < fSegmentLength || fSegmentLength==0))
			{
				//No segments
				if(fSegmentLength==0)
				{
					float fYAdd = 1 - fLength/ mfLengthTileSize;
                    textureView.Write(segmentIndex + 0, v2ToF2(cMath::ToForgeVec2(vTexCoords[0] - cVector2f(0,fYAdd))));
                    textureView.Write(segmentIndex + 1, v2ToF2(cMath::ToForgeVec2(vTexCoords[1] - cVector2f(0,fYAdd))));
                    textureView.Write(segmentIndex + 2, v2ToF2(cMath::ToForgeVec2(vTexCoords[2])));
                    textureView.Write(segmentIndex + 3, v2ToF2(cMath::ToForgeVec2(vTexCoords[3])));
				}
				//First segment of many
				else
				{
					float fYAdd = (1 - (fLength / fSegmentLength))*mfLengthTileAmount;

                    textureView.Write(segmentIndex + 0, v2ToF2(cMath::ToForgeVec2(vTexCoords[0] - cVector2f(0,fYAdd))));
                    textureView.Write(segmentIndex + 1, v2ToF2(cMath::ToForgeVec2(vTexCoords[1] - cVector2f(0,fYAdd))));
                    textureView.Write(segmentIndex + 2, v2ToF2(cMath::ToForgeVec2(vTexCoords[2])));
                    textureView.Write(segmentIndex + 3, v2ToF2(cMath::ToForgeVec2(vTexCoords[3])));
				}
			}
			else
			{
				for(int i=0; i<4; ++i) {
                    textureView.Write(segmentIndex + i, v2ToF2(cMath::ToForgeVec2(vTexCoords[i])));
			    }
			}

			//Update Normal and Tangent
			for(int i=0; i<4; ++i)
			{
                normalView.Write(i + segmentIndex, v3ToF3(cMath::ToForgeVec3(vFwd)));
			    tangentView.Write(i + segmentIndex, v3ToF3(cMath::ToForgeVec3(vRight)));
                colorView.Write(i + segmentIndex,float4(finalColor.r,finalColor.g,finalColor.b,finalColor.a));
			}

            segmentIndex++;

			//Update misc
			vPrevPos = vPos;
		}

        endUpdateResource(&positionUpdateDesc);
        endUpdateResource(&textureUpdateDesc);
        endUpdateResource(&colorUpdateDesc);
        endUpdateResource(&normalUpdateDesc);
        endUpdateResource(&tangentUpdateDesc);
		m_numberSegments = lCount - 1;

		return true;
	}

	cMatrixf* cRopeEntity::GetModelMatrix(cFrustum *apFrustum)
	{
		if(apFrustum==NULL)return &GetWorldMatrix();

		return NULL;
	}

	int cRopeEntity::GetMatrixUpdateCount()
	{
		return GetTransformUpdateCount();
	}

	bool cRopeEntity::IsVisible()
	{
		if(mColor.r <= 0 && mColor.g <= 0 && mColor.b <= 0) return false;

		return mbIsVisible;
	}

}
