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

#include "scene/BillBoard.h"

#include "graphics/GraphicsBuffer.h"
#include "graphics/MeshUtility.h"
#include "impl/LegacyVertexBuffer.h"
#include "impl/tinyXML/tinyxml.h"

#include "math/MathTypes.h"
#include "resources/Resources.h"
#include "resources/MaterialManager.h"
#include "resources/FileSearcher.h"

#include "graphics/VertexBuffer.h"
#include "graphics/Material.h"
#include "graphics/Graphics.h"
#include "graphics/LowLevelGraphics.h"
#include "graphics/Renderer.h"

#include "scene/Camera.h"
#include "scene/World.h"
#include "scene/Scene.h"
#include "math/Math.h"
#include "system/String.h"

#include "engine/Engine.h"

namespace hpl {

	cBillboard::cBillboard(const tString asName,const cVector2f& avSize,eBillboardType aType, cResources *apResources,cGraphics *apGraphics) :
	iRenderable(asName)
	{
		mpMaterialManager = apResources->GetMaterialManager();
		mpLowLevelGraphics = apGraphics->GetLowLevel();

		mvSize = avSize;
		mvAxis = cVector3f(0,1,0);

		mColor = cColor(1,1,1,1);
		mfForwardOffset =0;
		mfHaloAlpha = 1.0f;

		mType = aType;

		mpMaterial = NULL;

		mlLastRenderCount = -1;

        auto* graphicsAllocator = Interface<GraphicsAllocator>::Get();
        auto& opaqueSet = graphicsAllocator->resolveSet(GraphicsAllocator::AllocationSet::OpaqueSet);
        const uint32_t numberIndecies = 6 * (mType == eBillboardType_FixedAxis ? 2 : 1);
        m_geometry = opaqueSet.allocate(cBillboard::NumberVerts * 2, numberIndecies);

        {
            std::array<float3, cBillboard::NumberVerts> positionCoords = { float3((mvSize.x / 2), -(mvSize.y / 2), 0.0f),
                                                                           float3(-(mvSize.x / 2), -(mvSize.y / 2), 0.0f),
                                                                           float3(-(mvSize.x / 2), (mvSize.y / 2), 0.0f),
                                                                           float3((mvSize.x / 2), (mvSize.y / 2), 0.0f) };

            std::array<float2, cBillboard::NumberVerts> texcoords = { (float2(1.0f, -1.0f) + float2(1, 1)) / 2.0f, // Bottom left
                                                                      (float2(-1.0f, -1.0f) + float2(1, 1)) / 2.0f, // Bottom right
                                                                      (float2(-1.0f, 1.0f) + float2(1, 1)) / 2.0f, // Top left
                                                                      (float2(1.0f, 1.0f) + float2(1, 1)) / 2.0f }; // Top right

            auto& indexStream = m_geometry->indexBuffer();
            auto positionStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_POSITION);
            auto normalStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_NORMAL);
            auto colorStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_COLOR);
            auto textureStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_TEXCOORD0);
            auto tangentStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_TANGENT);

            BufferUpdateDesc indexUpdateDesc = { indexStream.m_handle, GeometrySet::IndexBufferStride * m_geometry->indexOffset(), GeometrySet::IndexBufferStride * numberIndecies };
            BufferUpdateDesc positionUpdateDesc = { positionStream->buffer().m_handle, positionStream->stride() * m_geometry->vertexOffset(), positionStream->stride() * cBillboard::NumberVerts * 2 };
            BufferUpdateDesc normalUpdateDesc = { normalStream->buffer().m_handle, normalStream->stride() * m_geometry->vertexOffset(), normalStream->stride() * cBillboard::NumberVerts * 2};
            BufferUpdateDesc colorUpdateDesc = { colorStream->buffer().m_handle, colorStream->stride() * m_geometry->vertexOffset(), colorStream->stride() * cBillboard::NumberVerts * 2};
            BufferUpdateDesc textureUpdateDesc = { textureStream->buffer().m_handle, textureStream->stride() * m_geometry->vertexOffset(), textureStream->stride() * cBillboard::NumberVerts * 2};
            BufferUpdateDesc tangentUpdateDesc = { tangentStream->buffer().m_handle, tangentStream->stride() * m_geometry->vertexOffset(), tangentStream->stride() * cBillboard::NumberVerts * 2};

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

            auto indexView = gpuIndexBuffer.CreateIndexView();
            {
                uint32_t index = 0;
                for (int i = 0; i < 3; i++) {
                    indexView.Write(index++, i);
                }
                for (int i = 2; i < 5; i++) {
                    indexView.Write(index++, i == 4 ? 0 : i);
                }
                if (mType == eBillboardType_FixedAxis) {
                    for (int i = 2; i >= 0; i--) {
                        indexView.Write(index++, i);
                    }
                    for (int i = 4; i >= 2; i--) {
                        indexView.Write(index++, i == 4 ? 0 : i);
                    }
                }

                for (size_t copyIdx = 0; copyIdx < 2; copyIdx++) {
                    auto positionView = gpuPositionBuffer.CreateStructuredView<float3>(positionStream->stride() * copyIdx * cBillboard::NumberVerts) ;
                    auto normalView = gpuNormalBuffer.CreateStructuredView<float3>(normalStream->stride() * copyIdx * cBillboard::NumberVerts);
                    auto colorView = gpuColorBuffer.CreateStructuredView<float4>(colorStream->stride() * copyIdx * cBillboard::NumberVerts);
                    auto textureView = gpuTextureBuffer.CreateStructuredView<float2>(textureStream->stride() * copyIdx * cBillboard::NumberVerts);
                    auto tangentView = gpuTangentBuffer.CreateStructuredView<float3>(tangentStream->stride() * copyIdx * cBillboard::NumberVerts);
                    for (size_t i = 0; i < cBillboard::NumberVerts; i++) {
                        positionView.Write(i, positionCoords[i]);
                        normalView.Write(i, float3(0.0f, 0.0f, 1.0f));
                        colorView.Write(i, float4(1,1,1,1));
                        textureView.Write(i, texcoords[i]);
                    }
                    Matrix4 trans = Matrix4::identity();
                    hpl::MeshUtility::MikkTSpaceGenerate(4, 6, &indexView, &positionView, &textureView, &normalView, &tangentView);
                }
            }

            endUpdateResource(&indexUpdateDesc);
            endUpdateResource(&positionUpdateDesc);
            endUpdateResource(&normalUpdateDesc);
            endUpdateResource(&colorUpdateDesc);
            endUpdateResource(&textureUpdateDesc);
            endUpdateResource(&tangentUpdateDesc);
        }
	   // mpVtxBuffer = mpLowLevelGraphics->CreateVertexBuffer(eVertexBufferType_Hardware,eVertexBufferDrawType_Tri, eVertexBufferUsageType_Dynamic,4,6);

	   // mpVtxBuffer->CreateElementArray(eVertexBufferElement_Position,eVertexBufferElementFormat_Float,4);
	   // mpVtxBuffer->CreateElementArray(eVertexBufferElement_Normal,eVertexBufferElementFormat_Float,3);
	   // mpVtxBuffer->CreateElementArray(eVertexBufferElement_Color0,eVertexBufferElementFormat_Float,4);
	   // mpVtxBuffer->CreateElementArray(eVertexBufferElement_Texture0,eVertexBufferElementFormat_Float,3);

	   // cVector3f vCoords[4] = {cVector3f((mvSize.x/2),-(mvSize.y/2),0),
	   // 						cVector3f(-(mvSize.x/2),-(mvSize.y/2),0),
	   // 						cVector3f(-(mvSize.x/2),(mvSize.y/2),0),
	   // 						cVector3f((mvSize.x/2),(mvSize.y/2),0)};

	   // cVector3f vTexCoords[4] = {cVector3f(1,-1,0),
	   // 						cVector3f(-1,-1,0),
	   // 						cVector3f(-1,1,0),
	   // 						cVector3f(1,1,0)};
	   // for(int i=0;i<4;i++)
	   // {
	   // 	mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Position, vCoords[i]);
	   // 	mpVtxBuffer->AddVertexColor(eVertexBufferElement_Color0, cColor(1,1,1,1));
	   // 	mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Texture0, (vTexCoords[i] + cVector2f(1,1))/2 );
	   // 	mpVtxBuffer->AddVertexVec3f(eVertexBufferElement_Normal,cVector3f(0,0,1));
	   // }

	   // for(int i=0;i<3;i++) mpVtxBuffer->AddIndex(i);
	   // for(int i=2;i<5;i++) mpVtxBuffer->AddIndex(i==4?0:i);

	   // //If the type is fixed, then we need a backside too
	   // //To do this, just all all the same indices in reversed order.
	   // if(mType == eBillboardType_FixedAxis)
	   // {
	   // 	for(int i=2; i>=0; i--) mpVtxBuffer->AddIndex(i);
	   // 	for(int i=4; i>=2; i--) mpVtxBuffer->AddIndex(i==4?0:i);
	   // }

	   // mpVtxBuffer->Compile(eVertexCompileFlag_CreateTangents);

		mbIsHalo = false;
		mvHaloSourceSize = 1;
		mpHaloSourceBV = NULL;
		mlHaloBVMatrixCount = -1;
		mbHaloSizeUpdated = true;

		mBoundingVolume.SetSize(cVector3f(mvSize.x, mvSize.y, mvSize.x));
	}

	//-----------------------------------------------------------------------

	cBillboard::~cBillboard()
	{
		if(mpMaterial) mpMaterialManager->Destroy(mpMaterial);
		//if(mpVtxBuffer) hplDelete(mpVtxBuffer);
		if(mpHaloSourceBV) hplDelete(mpHaloSourceBV);

	}


    DrawPacket cBillboard::ResolveDrawPacket(const ForgeRenderer::Frame& frame)  {
	    if((frame.m_frameIndex - m_frameIndex) >= ForgeRenderer::SwapChainLength - 1 && m_dirtyBuffer) {
	        m_frameIndex = frame.m_frameIndex;
	        m_activeCopy = (m_activeCopy + 1) % 2;
	        m_dirtyBuffer = false;

            auto positionStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_POSITION);
            auto colorStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_COLOR);
            auto textureStream = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_TEXCOORD0);

            BufferUpdateDesc positionUpdateDesc = { positionStream->buffer().m_handle, positionStream->stride() * (m_geometry->vertexOffset() + (cBillboard::NumberVerts * m_activeCopy)), positionStream->stride() * cBillboard::NumberVerts};
            BufferUpdateDesc textureUpdateDesc = { textureStream->buffer().m_handle, textureStream->stride() * (m_geometry->vertexOffset() + (cBillboard::NumberVerts * m_activeCopy)), textureStream->stride() * cBillboard::NumberVerts};
            BufferUpdateDesc colorUpdateDesc = { colorStream->buffer().m_handle, colorStream->stride() * (m_geometry->vertexOffset() + (cBillboard::NumberVerts * m_activeCopy)), colorStream->stride() * cBillboard::NumberVerts};

            beginUpdateResource(&positionUpdateDesc);
            beginUpdateResource(&textureUpdateDesc);
            beginUpdateResource(&colorUpdateDesc);

            GraphicsBuffer gpuPositionBuffer(positionUpdateDesc);
            GraphicsBuffer gpuTextureBuffer(textureUpdateDesc);
            GraphicsBuffer gpuColorBuffer(colorUpdateDesc);

            auto positionView = gpuPositionBuffer.CreateStructuredView<float3>();
            auto textureView = gpuTextureBuffer.CreateStructuredView<float2>();
            auto colorView = gpuColorBuffer.CreateStructuredView<float4>();

            std::array<float3, cBillboard::NumberVerts> positionCoords = { float3((mvSize.x / 2.0f), -(mvSize.y / 2.0f), 0.0f),
                                                                           float3(-(mvSize.x / 2.0f), -(mvSize.y / 2.0f), 0.0f),
                                                                           float3(-(mvSize.x / 2.0f), (mvSize.y / 2.0f), 0.0f),
                                                                           float3((mvSize.x / 2.0f), (mvSize.y / 2.0f), 0.0f) };

            std::array<float2, cBillboard::NumberVerts> texcoords = { (float2(1.0f, -1.0f) + float2(1, 1)) / 2.0f, // Bottom left
                                                                      (float2(-1.0f, -1.0f) + float2(1, 1)) / 2.0f, // Bottom right
                                                                      (float2(-1.0f, 1.0f) + float2(1, 1)) / 2.0f, // Top left
                                                                      (float2(1.0f, 1.0f) + float2(1, 1)) / 2.0f }; // Top right
            for (size_t i = 0; i < cBillboard::NumberVerts; i++) {
                positionView.Write(i, positionCoords[i]);
                colorView.Write(i, float4(mColor.r * mfHaloAlpha, mColor.g * mfHaloAlpha, mColor.b * mfHaloAlpha, mColor.a * mfHaloAlpha));
                textureView.Write(i, texcoords[i]);
            }

            endUpdateResource(&positionUpdateDesc);
            endUpdateResource(&textureUpdateDesc);
            endUpdateResource(&colorUpdateDesc);
	    }

        DrawPacket packet;

        DrawPacket::GeometrySetBinding binding{};
        packet.m_type = DrawPacket::DrawGeometryset;
        binding.m_subAllocation = m_geometry.get();
        binding.m_indexOffset = 0;
        binding.m_set = GraphicsAllocator::AllocationSet::OpaqueSet;
        binding.m_numIndices = 6 * (mType == eBillboardType_FixedAxis ? 2 : 0);
        binding.m_vertexOffset = (m_activeCopy * cBillboard::NumberVerts);
        packet.m_unified = binding;
        return packet;
    }

    void cBillboard::SetSize(const cVector2f& avSize)
	{
	    m_dirtyBuffer = true;
		mvSize = avSize;
		mBoundingVolume.SetSize(cVector3f(mvSize.x, mvSize.y, mvSize.x));

	   // float *pPos = mpVtxBuffer->GetFloatArray(eVertexBufferElement_Position);

	   // cVector3f vCoords[4] = {cVector3f((mvSize.x/2),-(mvSize.y/2),0),
	   // 						cVector3f(-(mvSize.x/2),-(mvSize.y/2),0),
	   // 						cVector3f(-(mvSize.x/2),(mvSize.y/2),0),
	   // 						cVector3f((mvSize.x/2),(mvSize.y/2),0)};

	   // for(int i=0; i<4;++i)
	   // {
	   // 	pPos[0] = vCoords[i].x;
	   // 	pPos[1] = vCoords[i].y;
	   // 	pPos[2] = vCoords[i].z;
	   // 	pPos+=4;
	   // }

	   // mpVtxBuffer->UpdateData(eVertexElementFlag_Position,false);

		if(mType == eBillboardType_Axis) {
		    SetAxis(mvAxis);
	    }

		SetTransformUpdated();
	}

	//-----------------------------------------------------------------------

	void cBillboard::SetAxis(const cVector3f& avAxis)
	{
		mvAxis = avAxis;
		mvAxis.Normalize();

		//This is a quick fix so the bounding box is correct for non up-pointing axises
		if(mType == eBillboardType_Axis && mvAxis != cVector3f(0,1,0))
		{
			float fMax = mvSize.x;
			if(fMax < mvSize.y) fMax = mvSize.y;

			fMax *= kSqrt2f;

			mBoundingVolume.SetSize(fMax);

			SetTransformUpdated();
		}
	}

	//-----------------------------------------------------------------------

	void cBillboard::SetColor(const cColor &aColor)
	{
		if(mColor == aColor) return;
        m_dirtyBuffer = true;
		mColor = aColor;

	   // float *pColors = mpVtxBuffer->GetFloatArray(eVertexBufferElement_Color0);

	   // for(int i=0; i<4;++i)
	   // {
	   // 	pColors[0] = mColor.r * mfHaloAlpha;
	   // 	pColors[1] = mColor.g * mfHaloAlpha;
	   // 	pColors[2] = mColor.b * mfHaloAlpha;
	   // 	pColors[3] = mColor.a * mfHaloAlpha;
	   // 	pColors+=4;
	   // }

	   // mpVtxBuffer->UpdateData(eVertexElementFlag_Color0,false);
	}

	//-----------------------------------------------------------------------

	void cBillboard::SetHaloAlpha(float afX)
	{
		if(mfHaloAlpha == afX)
		{
			return;
		}

		mfHaloAlpha = afX;
        m_dirtyBuffer = true;

	 //   float *pColors = mpVtxBuffer->GetFloatArray(eVertexBufferElement_Color0);

	 //   for(int i=0; i<4;++i)
	 //   {
	 //   	pColors[0] = mColor.r * mfHaloAlpha;
	 //   	pColors[1] = mColor.g * mfHaloAlpha;
	 //   	pColors[2] = mColor.b * mfHaloAlpha;
	 //   	pColors[3] = mColor.a * mfHaloAlpha;
	 //   	pColors+=4;
	 //   }

	 //   mpVtxBuffer->UpdateData(eVertexElementFlag_Color0,false);
	}

	//-----------------------------------------------------------------------

	void cBillboard::SetForwardOffset(float afOffset)
	{
		mfForwardOffset = afOffset;
	}

	//-----------------------------------------------------------------------

	void cBillboard::SetMaterial(cMaterial * apMaterial)
	{
		mpMaterial = apMaterial;
	}

	//-----------------------------------------------------------------------

	cMatrixf* cBillboard::GetModelMatrix(cFrustum *apFrustum)
	{
		if(apFrustum==NULL)return &GetWorldMatrix();

		//////////////////////
		// Fixed Axis
		if(mType == eBillboardType_FixedAxis)
		{
			return &GetWorldMatrix();
		}

		//////////////////////
		// Set up for all rotating billboards
		m_mtxTempTransform = GetWorldMatrix();
		cVector3f vForward, vRight, vUp;

		cVector3f vCameraForward = apFrustum->GetOrigin() - GetWorldPosition();
		vCameraForward.Normalize();

		//////////////////////
		// Point
		if(mType == eBillboardType_Point)
		{
			vForward = vCameraForward;
			vRight = cMath::Vector3Cross(apFrustum->GetViewMatrix().GetUp(), vForward);
			vUp = cMath::Vector3Cross(vForward,vRight);
		}
		//////////////////////
		// Axis
		else if(mType == eBillboardType_Axis)
		{
			vUp = cMath::MatrixMul(GetWorldMatrix().GetRotation(),mvAxis);
			vUp.Normalize();

			if(vUp == vCameraForward)
			{
				vRight = cMath::Vector3Cross(vUp, vCameraForward);
				Warning("Billboard Right vector is not correct! Contact programmer!\n");
			}
			else
				vRight = cMath::Vector3Cross(vUp, vCameraForward);

			vRight.Normalize();
			vForward = cMath::Vector3Cross(vRight, vUp);

			//vForward.Normalize();
			//vUp.Normalize();
		}

		if(mfForwardOffset!=0)
		{
			cVector3f vPos = m_mtxTempTransform.GetTranslation();
			vPos +=  vCameraForward * mfForwardOffset;
			m_mtxTempTransform.SetTranslation(vPos);
		}

		//Set right vector
		m_mtxTempTransform.m[0][0] = vRight.x;
		m_mtxTempTransform.m[1][0] = vRight.y;
		m_mtxTempTransform.m[2][0] = vRight.z;

		//Set up vector
		m_mtxTempTransform.m[0][1] = vUp.x;
		m_mtxTempTransform.m[1][1] = vUp.y;
		m_mtxTempTransform.m[2][1] = vUp.z;

		//Set forward vector
		m_mtxTempTransform.m[0][2] = vForward.x;
		m_mtxTempTransform.m[1][2] = vForward.y;
		m_mtxTempTransform.m[2][2] = vForward.z;

		return &m_mtxTempTransform;
	}

	//-----------------------------------------------------------------------

	int cBillboard::GetMatrixUpdateCount()
	{
		return GetTransformUpdateCount();
	}

	//-----------------------------------------------------------------------


	bool cBillboard::IsVisible()
	{
		if(mColor.r <= 0 && mColor.g <= 0 && mColor.b <= 0) return false;

		return mbIsVisible;
	}

	//-----------------------------------------------------------------------

	void cBillboard::SetIsHalo(bool abX)
	{
		mbIsHalo = abX;


		if(mbIsHalo)
		{
			mfHaloAlpha = 1; //THis is to make sure that the new alpha is set to the mesh.
			SetHaloAlpha(0);

			//Create source bv
			if(mpHaloSourceBV == NULL) mpHaloSourceBV = hplNew(cBoundingVolume, ());
		}
		else
		{
			//Delete source bv
			if(mpHaloSourceBV)
			{
				hplDelete(mpHaloSourceBV);
				mpHaloSourceBV = NULL;
			}
		}
	}

	//-----------------------------------------------------------------------

	void cBillboard::SetHaloSourceSize(const cVector3f &avSize)
	{
		mvHaloSourceSize = avSize;

		mbHaloSizeUpdated = true;
	}

	//-----------------------------------------------------------------------

	bool cBillboard::UsesOcclusionQuery()
	{
		return mbIsHalo;
	}


    float cBillboard::getAreaOfScreenSpace(cFrustum* frustum) {
        // Update bv size
        if (mbHaloSizeUpdated) {
                mpHaloSourceBV->SetSize(mvHaloSourceSize);
                mbHaloSizeUpdated = false;
        }

        // Update bv transform
        if (mlHaloBVMatrixCount != GetTransformUpdateCount()) {
                mpHaloSourceBV->SetTransform(GetWorldMatrix());
                mlHaloBVMatrixCount = GetTransformUpdateCount();
        }

        // Get the screen clip rect and see how much is inside screen.
        cVector3f vMin, vMax;
        if (cMath::GetNormalizedClipRectFromBV(vMin, vMax, *mpHaloSourceBV, frustum, 0)) {
                cVector3f vTotalSize = vMax - vMin;

                if (vMin.x < -1)
                        vMin.x = -1;
                if (vMin.y < -1)
                        vMin.y = -1;
                if (vMax.x > 1)
                        vMax.x = 1;
                if (vMax.y > 1)
                        vMax.y = 1;

                cVector3f vInsideSize = vMax - vMin;

                float fInsideArea = vInsideSize.x * vInsideSize.y;
                float fTotalArea = vTotalSize.x * vTotalSize.y;

                if (fTotalArea > 0) {
                        return fInsideArea / fTotalArea;
                }
        }
        return 0;
    }

}
