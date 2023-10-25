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


#include "graphics/ForgeRenderer.h"
#include "graphics/GraphicsBuffer.h"
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

#include "math/MathTypes.h"
#include "scene/AnimationState.h"
#include "scene/NodeState.h"
#include "scene/MeshEntity.h"
#include "scene/SubMeshEntity.h"

#include "resources/MaterialManager.h"
#include "resources/MeshManager.h"

#include "physics/PhysicsBody.h"

#include "math/Math.h"
#include <algorithm>
#include <cstdint>

namespace hpl {
    cSubMeshEntity::cSubMeshEntity(
        const tString& asName, cMeshEntity* apMeshEntity, cSubMesh* apSubMesh, cMaterialManager* apMaterialManager)
        : iRenderable(asName)
        , mpMeshEntity(apMeshEntity)
        , mpSubMesh(apSubMesh)
        , mpMaterialManager(apMaterialManager) {
        mbIsOneSided = mpSubMesh->GetIsOneSided();
        mvOneSidedNormal = mpSubMesh->GetOneSidedNormal();
        if (mpMeshEntity->GetMesh()->GetSkeleton()) {
            mpDynVtxBuffer =
                mpSubMesh->GetVertexBuffer()->CreateCopy(eVertexBufferType_Hardware, eVertexBufferUsageType_Dynamic, eFlagBit_All);
        }

        m_skinnedMesh  = mpMeshEntity->GetMesh()->GetSkeleton();
        for (auto& stream : mpSubMesh->streamBuffers()) {
            auto rawView = stream.m_buffer.CreateViewRaw();
            StreamBufferInfo& info = m_vertexStreams.emplace_back();
            info.m_stride = stream.m_stride;
            info.m_semantic = stream.m_semantic;
            info.m_numberElements = stream.m_numberElements;
            if(m_skinnedMesh
                && (stream.m_semantic == ShaderSemantic::SEMANTIC_POSITION ||
                    stream.m_semantic == ShaderSemantic::SEMANTIC_TANGENT ||
                    stream.m_semantic == ShaderSemantic::SEMANTIC_NORMAL
                )) {
                    info.m_type = StreamBufferType::DynamicBuffer;
                    info.m_buffer.Load([&](Buffer** buffer) {
                        BufferLoadDesc loadDesc = {};
                        loadDesc.ppBuffer = buffer;
                        loadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
                        loadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
                        loadDesc.mDesc.mSize = rawView.NumBytes() * ForgeRenderer::SwapChainLength; // will have a copy per frame
                        loadDesc.pData = rawView.rawByteSpan().data();
                        addResource(&loadDesc, nullptr);
                        return true;
                    });
            } else {
                info.m_type = StreamBufferType::StaticBuffer;
                info.m_buffer = stream.CommitBuffer();
                // info.m_buffer.Load([&](Buffer** buffer) {
               //     BufferLoadDesc loadDesc = {};
               //     loadDesc.ppBuffer = buffer;
               //     loadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
               //     loadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
               //     loadDesc.mDesc.mSize = rawView.NumBytes();
               //     loadDesc.pData = rawView.rawByteSpan().data();
               //     addResource(&loadDesc, nullptr);
               //     return true;
               // });
            }
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
    }

    cSubMeshEntity::~cSubMeshEntity() {
        if (mpDynVtxBuffer)
            hplDelete(mpDynVtxBuffer);

        /* Clear any custom textures here*/
        if (mpMaterial)
            mpMaterialManager->Destroy(mpMaterial);
    }

    void cSubMeshEntity::UpdateLogic(float afTimeStep) {
    }

    cMaterial* cSubMeshEntity::GetMaterial() {
        if (mpMaterial == NULL && mpSubMesh->GetMaterial() == NULL) {
            // Error("Materials for sub entity %s are NULL!\n",GetName().c_str());
        }

        if (mpMaterial)
            return mpMaterial;
        else
            return mpSubMesh->GetMaterial();
    }

    static inline float3 WeightTransform(const cMatrixf& a_mtxA, float3 src, float weight) {

        return float3((a_mtxA.m[0][0] * src[0] + a_mtxA.m[0][1] * src[1] + a_mtxA.m[0][2] * src[2] + a_mtxA.m[0][3]) * weight,
                      (a_mtxA.m[1][0] * src[0] + a_mtxA.m[1][1] * src[1] + a_mtxA.m[1][2] * src[2] + a_mtxA.m[1][3]) * weight,
                      (a_mtxA.m[2][0] * src[0] + a_mtxA.m[2][1] * src[1] + a_mtxA.m[2][2] * src[2] + a_mtxA.m[2][3]) * weight);

    }
    static inline float3 WeightTransformRotation(const cMatrixf& a_mtxA, float3 src, float weight) {

        return float3((a_mtxA.m[0][0] * src[0] + a_mtxA.m[0][1] * src[1] + a_mtxA.m[0][2] * src[2]) * weight,
                      (a_mtxA.m[1][0] * src[0] + a_mtxA.m[1][1] * src[1] + a_mtxA.m[1][2] * src[2]) * weight,
                      (a_mtxA.m[2][0] * src[0] + a_mtxA.m[2][1] * src[1] + a_mtxA.m[2][2] * src[2]) * weight);

    }


    void cSubMeshEntity::UpdateGraphicsForFrame(float afFrameTime) {
        ////////////////////////////////////
        // Update things in parent first.
        mpMeshEntity->UpdateGraphicsForFrame(afFrameTime);

        ////////////////////////////////////
        // If it has dynamic mesh, update it.
        if (m_skinnedMesh) {
            if (mpMeshEntity->mbSkeletonPhysicsSleeping && mbGraphicsUpdated) {
                return;
            }

            m_activeCopy = (m_activeCopy + 1) % ForgeRenderer::SwapChainLength;
            mbGraphicsUpdated = true;

            auto staticBindBuffers = mpSubMesh->streamBuffers();
            auto bindPositionIt = std::find_if(staticBindBuffers.begin(), staticBindBuffers.end(), [&](auto& stream) {
                return stream.m_semantic == ShaderSemantic::SEMANTIC_POSITION;
            });
            auto bindNormalIt = std::find_if(staticBindBuffers.begin(), staticBindBuffers.end(), [&](auto& stream) {
                return stream.m_semantic == ShaderSemantic::SEMANTIC_NORMAL;
            });
            auto bindTangentIt = std::find_if(staticBindBuffers.begin(), staticBindBuffers.end(), [&](auto& stream) {
                return stream.m_semantic == ShaderSemantic::SEMANTIC_TANGENT;
            });
            ASSERT(bindPositionIt != staticBindBuffers.end() &&
                bindNormalIt != staticBindBuffers.end()  &&
                bindTangentIt != staticBindBuffers.end()
            );
            auto bindPositonView = bindPositionIt->GetStructuredView<float3>(); // the static buffer data is the fixed pose
            auto bindNormalView = bindNormalIt->GetStructuredView<float3>();
            auto bindTangentView = bindTangentIt->GetStructuredView<float3>();

            auto targetPositionIt = std::find_if(m_vertexStreams.begin(), m_vertexStreams.end(), [&](auto& stream) {
                return stream.m_semantic == ShaderSemantic::SEMANTIC_POSITION;
            });
            auto targetNormalIt = std::find_if(m_vertexStreams.begin(), m_vertexStreams.end(), [&](auto& stream) {
                return stream.m_semantic == ShaderSemantic::SEMANTIC_NORMAL;
            });
            auto targetTangentIt = std::find_if(m_vertexStreams.begin(), m_vertexStreams.end(), [&](auto& stream) {
                return stream.m_semantic == ShaderSemantic::SEMANTIC_TANGENT;
            });

            ASSERT(targetPositionIt != m_vertexStreams.end() &&
                targetNormalIt != m_vertexStreams.end()  &&
                targetTangentIt != m_vertexStreams.end()
            );
            BufferUpdateDesc positionUpdateDesc = { targetPositionIt->m_buffer.m_handle, m_activeCopy * (targetPositionIt->m_stride * targetPositionIt->m_numberElements), targetPositionIt->m_stride * targetPositionIt->m_numberElements};
            BufferUpdateDesc tangentUpdateDesc = { targetTangentIt->m_buffer.m_handle, m_activeCopy * (targetTangentIt->m_stride * targetTangentIt->m_numberElements), targetTangentIt->m_stride * targetTangentIt->m_numberElements };
            BufferUpdateDesc normalUpdateDesc = { targetNormalIt->m_buffer.m_handle, m_activeCopy * (targetNormalIt->m_stride * targetNormalIt->m_numberElements), targetNormalIt->m_stride * targetNormalIt->m_numberElements };
            beginUpdateResource(&positionUpdateDesc);
            beginUpdateResource(&tangentUpdateDesc);
            beginUpdateResource(&normalUpdateDesc);

            GraphicsBuffer positionMapping(positionUpdateDesc);
            GraphicsBuffer normalMapping(tangentUpdateDesc);
            GraphicsBuffer tangentMapping(normalUpdateDesc);
            auto targetPositionView = positionMapping.CreateStructuredView<float3>(0, targetPositionIt->m_stride);
            auto targetTangentView = tangentMapping.CreateStructuredView<float3>(0, targetTangentIt->m_stride);
            auto targetNormalView = normalMapping.CreateStructuredView<float3>(0, targetNormalIt->m_stride);

            for(size_t i = 0; i < bindPositionIt->m_numberElements; i++) {

                const std::span<float> weights = std::span<float>(&mpSubMesh->m_vertexWeights[(i * 4)], 4);
                const std::span<uint8_t> boneIdxs = std::span<uint8_t>(&mpSubMesh->m_vertexBones[(i * 4)], 4);

                float3 bindPos = bindPositonView.Get(i);
                float3 bindNormal = bindNormalView.Get(i);
                float3 bindTangent = bindTangentView.Get(i);

                float3 accmulatedPos = float3(0);
                float3 accmulatedNormal = float3(0);
                float3 accmulatedTangent = float3(0);

                for(size_t boneIdx = 0; boneIdx < 4 && weights[boneIdx] != 0; boneIdx++) {
                    const float weight = weights[boneIdx];
                    const cMatrixf& transform = mpMeshEntity->mvBoneMatrices[boneIdxs[boneIdx]];
                    accmulatedPos += WeightTransform(transform, bindPos, weight);
                    accmulatedNormal += WeightTransformRotation(transform, bindNormal, weight);
                    accmulatedTangent += WeightTransformRotation(transform, bindTangent, weight);
                }
                targetPositionView.Write(i, accmulatedPos);
                targetNormalView.Write(i, accmulatedNormal);
                targetTangentView.Write(i, accmulatedTangent);
            }

            endUpdateResource(&positionUpdateDesc, nullptr);
            endUpdateResource(&tangentUpdateDesc, nullptr);
            endUpdateResource(&normalUpdateDesc, nullptr);
		}

	}


    DrawPacket cSubMeshEntity::ResolveDrawPacket(const ForgeRenderer::Frame& frame,std::span<eVertexBufferElement> elements)  {
		DrawPacket packet;
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
	        if(found->m_type == StreamBufferType::DynamicBuffer) {
	            stream.m_offset = m_activeCopy * (found->m_numberElements * found->m_stride);
	        }
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

} // namespace hpl
