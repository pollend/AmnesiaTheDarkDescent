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

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "FixPreprocessor.h"


#include "graphics/VertexBuffer.h"
#include "graphics/Material.h"
#include "graphics/Mesh.h"
#include "graphics/SubMesh.h"
#include "graphics/Animation.h"
#include "graphics/AnimationTrack.h"
#include "graphics/Skeleton.h"
#include "graphics/Bone.h"
#include "graphics/DrawPacket.h"
#include "graphics/Enum.h"
#include "graphics/ForgeHandles.h"
#include "impl/LegacyVertexBuffer.h"

#include "scene/AnimationState.h"
#include "scene/NodeState.h"
#include "scene/MeshEntity.h"
#include "scene/SubMeshEntity.h"

#include "resources/MaterialManager.h"
#include "resources/MeshManager.h"

#include "physics/PhysicsBody.h"

#include "math/Math.h"
#include <algorithm>


namespace hpl {
	cSubMeshEntity::cSubMeshEntity(const tString &asName, cMeshEntity *apMeshEntity, cSubMesh * apSubMesh,
								cMaterialManager* apMaterialManager) : iRenderable(asName)
	{
		mpMeshEntity = apMeshEntity;
		mpSubMesh = apSubMesh;

		mbIsOneSided = mpSubMesh->GetIsOneSided();
		mvOneSidedNormal = mpSubMesh->GetOneSidedNormal();

		mpMaterialManager = apMaterialManager;

		mbGraphicsUpdated = false;

		if(mpMeshEntity->GetMesh()->GetSkeleton())
		{
			mpDynVtxBuffer = mpSubMesh->GetVertexBuffer()->CreateCopy(eVertexBufferType_Hardware,eVertexBufferUsageType_Dynamic,eFlagBit_All);
		}

        //if(mpSubMesh->hasMesh()) {
            for(auto& stream: mpSubMesh->streamBuffers()) {
                auto rawView = stream.m_buffer.CreateViewRaw();
                StreamBufferInfo& info = m_vertexStreams.emplace_back();
                info.m_buffer.Load([&](Buffer** buffer) {
                    BufferLoadDesc loadDesc = {};
                    loadDesc.ppBuffer = buffer;
                    loadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
                    loadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
                    loadDesc.mDesc.mSize = rawView.NumBytes();
                    loadDesc.pData = rawView.rawByteSpan().data();
                    addResource(&loadDesc, nullptr);
                    return true;
                });
                info.m_stride = stream.m_stride;
                info.m_semantic = stream.m_semantic;
            }
            {
                auto& info = mpSubMesh->IndexStream();
                auto rawView = info.m_buffer.CreateViewRaw();
                m_indexBuffer.Load([&](Buffer** buffer) {
                    BufferLoadDesc loadDesc = {};
                    loadDesc.ppBuffer = buffer;
                    loadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
                    loadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
                    loadDesc.mDesc.mSize = rawView.NumBytes();
                    loadDesc.pData = rawView.rawByteSpan().data();
                    addResource(&loadDesc, nullptr);
                    return true;
                });
                m_numberIndecies = info.m_numberElements;
            }
        //}

		mpLocalNode = NULL;

		mbUpdateBody = false;

		mpMaterial = NULL;

		mpUserData = NULL;

		//This is used to see if null should be returned.
		// 0 = no check made, test if matrix is identity
		// -1 = Matrix was not identity
		// 1 = matrix was identiy
		mlStaticNullMatrixCount =0;
	}

	cSubMeshEntity::~cSubMeshEntity()
	{
		if(mpDynVtxBuffer) hplDelete(mpDynVtxBuffer);

		/* Clear any custom textures here*/
		if(mpMaterial) mpMaterialManager->Destroy(mpMaterial);
	}

	void cSubMeshEntity::UpdateLogic(float afTimeStep)
	{

	}


	cMaterial* cSubMeshEntity::GetMaterial()
	{
		if(mpMaterial==NULL && mpSubMesh->GetMaterial()==NULL)
		{
			//Error("Materials for sub entity %s are NULL!\n",GetName().c_str());
		}

		if(mpMaterial)
			return mpMaterial;
		else
			return mpSubMesh->GetMaterial();
	}

	//-----------------------------------------------------------------------

	// Set Src as private variable to give this a little boost! Or?
	static inline void MatrixFloatTransformSet(float *pDest, const cMatrixf &a_mtxA, const float* pSrc, const float fWeight)
	{
		pDest[0] = ( a_mtxA.m[0][0] * pSrc[0] + a_mtxA.m[0][1] * pSrc[1] + a_mtxA.m[0][2] * pSrc[2] + a_mtxA.m[0][3] ) * fWeight;
		pDest[1] = ( a_mtxA.m[1][0] * pSrc[0] + a_mtxA.m[1][1] * pSrc[1] + a_mtxA.m[1][2] * pSrc[2] + a_mtxA.m[1][3] ) * fWeight;
		pDest[2] = ( a_mtxA.m[2][0] * pSrc[0] + a_mtxA.m[2][1] * pSrc[1] + a_mtxA.m[2][2] * pSrc[2] + a_mtxA.m[2][3] ) * fWeight;
	}

	static inline void MatrixFloatRotateSet(float *pDest, const cMatrixf &a_mtxA, const float* pSrc, const float fWeight)
	{
		pDest[0] = ( a_mtxA.m[0][0] * pSrc[0] + a_mtxA.m[0][1] * pSrc[1] + a_mtxA.m[0][2] * pSrc[2] ) * fWeight;
		pDest[1] = ( a_mtxA.m[1][0] * pSrc[0] + a_mtxA.m[1][1] * pSrc[1] + a_mtxA.m[1][2] * pSrc[2] ) * fWeight;
		pDest[2] = ( a_mtxA.m[2][0] * pSrc[0] + a_mtxA.m[2][1] * pSrc[1] + a_mtxA.m[2][2] * pSrc[2] ) * fWeight;
	}

	//-----------------------------------------------------------------------

	// Set Src as private variable to give this a little boost!Or?
	static inline void MatrixFloatTransformAdd(float *pDest, const cMatrixf &a_mtxA, const float* pSrc, const float fWeight)
	{
		pDest[0] += ( a_mtxA.m[0][0] * pSrc[0] + a_mtxA.m[0][1] * pSrc[1] + a_mtxA.m[0][2] * pSrc[2] + a_mtxA.m[0][3] ) * fWeight;
		pDest[1] += ( a_mtxA.m[1][0] * pSrc[0] + a_mtxA.m[1][1] * pSrc[1] + a_mtxA.m[1][2] * pSrc[2] + a_mtxA.m[1][3] ) * fWeight;
		pDest[2] += ( a_mtxA.m[2][0] * pSrc[0] + a_mtxA.m[2][1] * pSrc[1] + a_mtxA.m[2][2] * pSrc[2] + a_mtxA.m[2][3] ) * fWeight;
	}

	static inline void MatrixFloatRotateAdd(float *pDest, const cMatrixf &a_mtxA, const float* pSrc, const float fWeight)
	{
		pDest[0] += ( a_mtxA.m[0][0] * pSrc[0] + a_mtxA.m[0][1] * pSrc[1] + a_mtxA.m[0][2] * pSrc[2] ) * fWeight;
		pDest[1] += ( a_mtxA.m[1][0] * pSrc[0] + a_mtxA.m[1][1] * pSrc[1] + a_mtxA.m[1][2] * pSrc[2] ) * fWeight;
		pDest[2] += ( a_mtxA.m[2][0] * pSrc[0] + a_mtxA.m[2][1] * pSrc[1] + a_mtxA.m[2][2] * pSrc[2] ) * fWeight;
	}

	//-----------------------------------------------------------------------

	void cSubMeshEntity::UpdateGraphicsForFrame(float afFrameTime)
	{
		////////////////////////////////////
		//Update things in parent first.
		mpMeshEntity->UpdateGraphicsForFrame(afFrameTime);

		////////////////////////////////////
		// If it has dynamic mesh, update it.
		if(mpDynVtxBuffer)
		{
			if(mpMeshEntity->mbSkeletonPhysicsSleeping && mbGraphicsUpdated)
			{
				return;
			}

			mbGraphicsUpdated = true;

			const float *pBindPos = mpSubMesh->GetVertexBuffer()->GetFloatArray(eVertexBufferElement_Position);
			const float *pBindNormal = mpSubMesh->GetVertexBuffer()->GetFloatArray(eVertexBufferElement_Normal);
			const float *pBindTangent = mpSubMesh->GetVertexBuffer()->GetFloatArray(eVertexBufferElement_Texture1Tangent);

			float *pSkinPos = mpDynVtxBuffer->GetFloatArray(eVertexBufferElement_Position);
			float *pSkinNormal = mpDynVtxBuffer->GetFloatArray(eVertexBufferElement_Normal);
			float *pSkinTangent = mpDynVtxBuffer->GetFloatArray(eVertexBufferElement_Texture1Tangent);

			const int lVtxStride = mpDynVtxBuffer->GetElementNum(eVertexBufferElement_Position);
			const int lVtxNum = mpDynVtxBuffer->GetVertexNum();

			for(int vtx=0; vtx < lVtxNum; vtx++)
			{
				//To count the bone bindings
				int lCount = 0;
				//Get pointer to weights and bone index.
			    if(mpSubMesh->m_vertexWeights.size() == 0) {
                    continue;
			    }
				const float *pWeight = &mpSubMesh->m_vertexWeights[vtx*4];
				const unsigned char *pBoneIdx = &mpSubMesh->m_vertexBones[vtx*4];

				const cMatrixf &mtxTransform = mpMeshEntity->mvBoneMatrices[*pBoneIdx];


				MatrixFloatTransformSet(pSkinPos,mtxTransform, pBindPos, *pWeight);

				MatrixFloatRotateSet(pSkinNormal,mtxTransform, pBindNormal, *pWeight);

				MatrixFloatRotateSet(pSkinTangent,mtxTransform, pBindTangent, *pWeight);

				++pWeight; ++pBoneIdx; ++lCount;

				//Iterate weights until 0 is found or count < 4
				while(*pWeight != 0 && lCount < 4)
				{
					//Log("Boneidx: %d Count %d Weight: %f\n",(int)*pBoneIdx,lCount, *pWeight);
					const cMatrixf &mtxTransform = mpMeshEntity->mvBoneMatrices[*pBoneIdx];

					//Transform with the local movement of the bone.
					MatrixFloatTransformAdd(pSkinPos,mtxTransform, pBindPos, *pWeight);

					MatrixFloatRotateAdd(pSkinNormal,mtxTransform, pBindNormal, *pWeight);

					MatrixFloatRotateAdd(pSkinTangent,mtxTransform, pBindTangent, *pWeight);

					++pWeight; ++pBoneIdx; ++lCount;
				}

				pBindPos += lVtxStride;
				pSkinPos += lVtxStride;

				pBindNormal += 3;
				pSkinNormal += 3;

				pBindTangent += 4;
				pSkinTangent += 4;
			}
			//Update buffer
			mpDynVtxBuffer->UpdateData(eVertexElementFlag_Position | eVertexElementFlag_Normal | eVertexElementFlag_Texture1,false);
		}

	}


    DrawPacket cSubMeshEntity::ResolveDrawPacket(const ForgeRenderer::Frame& frame,std::span<eVertexBufferElement> elements)  {
		DrawPacket packet;
	    packet.m_type = DrawPacket::Unknown;
	    if(mpDynVtxBuffer)
		{
	        return static_cast<LegacyVertexBuffer*>(mpDynVtxBuffer)->resolveGeometryBinding(frame.m_currentFrame, elements);
	    }
	    if(m_numberIndecies == 0) {
            return packet;
	    }

	    packet.m_type = DrawPacket::IndvidualBindings;
	    packet.m_indvidual.m_numStreams = 0;
	    for(auto& ele: elements) {
	       ShaderSemantic semantic = hplToForgeShaderSemantic(ele);
            auto found = std::find_if(m_vertexStreams.begin(), m_vertexStreams.end(), [&](auto& stream) {
                return stream.m_semantic == semantic;
            });
            ASSERT(found != m_vertexStreams.end());
	        auto& stream = packet.m_indvidual.m_vertexStream[packet.m_indvidual.m_numStreams++];
	        stream.m_buffer = &found->m_buffer;
	        stream.m_offset = 0;
	        stream.m_stride = found->m_stride;
	    }
	    packet.m_indvidual.m_indexStream.m_offset = 0;
	    packet.m_indvidual.m_indexStream.buffer = &m_indexBuffer;
	    packet.m_numIndices = m_numberIndecies;
	    return packet;
    }
	iVertexBuffer* cSubMeshEntity::GetVertexBuffer()
	{
		if(mpDynVtxBuffer)
		{
			return mpDynVtxBuffer;
		}
		else
		{
			return mpSubMesh->GetVertexBuffer();
		}
	}

	cBoundingVolume* cSubMeshEntity::GetBoundingVolume()
	{
		if(mpMeshEntity->GetMesh()->GetSkeleton())
		{
			return mpMeshEntity->GetBoundingVolume();
		}
		else
		{
			if(mbUpdateBoundingVolume)
			{
				mBoundingVolume.SetTransform(GetWorldMatrix());
				mbUpdateBoundingVolume = false;
			}

			return &mBoundingVolume;
		}
	}

	int cSubMeshEntity::GetMatrixUpdateCount()
	{
		return GetTransformUpdateCount();
	}

	cMatrixf* cSubMeshEntity::GetModelMatrix(cFrustum *apFrustum)
	{
		////////////////////////
		// Static entity
		if(IsStatic() && mlStaticNullMatrixCount>=0)
		{
			if(mlStaticNullMatrixCount==0)
			{
				if(GetWorldMatrix() == cMatrixf::Identity) 	mlStaticNullMatrixCount = 1;
				else										mlStaticNullMatrixCount = -1;

				if(mlStaticNullMatrixCount == 1)return NULL;
				else							return &GetWorldMatrix();

			}
			else
			{
				return NULL;
			}
		}
		////////////////////////
		// Dynamic
		else
		{
			return &GetWorldMatrix();
		}
	}

	void cSubMeshEntity::SetLocalNode(cNode3D *apNode)
	{
		mpLocalNode = apNode;

		mpLocalNode->AddEntity(this);
	}

	cNode3D* cSubMeshEntity::GetLocalNode()
	{
		return mpLocalNode;
	}


	void cSubMeshEntity::SetUpdateBody(bool abX)
	{
		mbUpdateBody = abX;
	}

	bool cSubMeshEntity::GetUpdateBody()
	{
		return mbUpdateBody;
	}

	void cSubMeshEntity::SetCustomMaterial(cMaterial *apMaterial, bool abDestroyOldCustom)
	{
		if(abDestroyOldCustom)
		{
			if(mpMaterial) mpMaterialManager->Destroy(mpMaterial);
		}

        mpMaterial = apMaterial;
	}

	void cSubMeshEntity::OnTransformUpdated()
	{
		mpMeshEntity->mbUpdateBoundingVolume = true;
	}

}
