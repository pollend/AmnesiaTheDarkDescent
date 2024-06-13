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
#include "graphics/GraphicsAllocator.h"
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

    void cSubMeshEntity::updateVertexStream(cSubMesh::StreamBufferInfo& localStream) {
        auto gpuStream = m_geometry->getStreamBySemantic(localStream.m_semantic);
        ASSERT(gpuStream != m_geometry->vertexStreams().end());
        const uint32_t reservedVerticies = m_numberVertices * (m_isSkinnedMesh ? 2 : 1);

        BufferUpdateDesc updateDesc = { gpuStream->buffer().m_handle,
                                        gpuStream->stride() * m_geometry->vertexOffset(),
                                        gpuStream->stride() * reservedVerticies };
        beginUpdateResource(&updateDesc);

        GraphicsBuffer gpuBuffer(updateDesc);
        auto dest = gpuBuffer.CreateViewRaw();
        auto src = localStream.m_buffer.CreateViewRaw();
        for (size_t i = 0; i < m_numberVertices; i++) {
            auto sp = src.rawByteSpan().subspan(i * localStream.m_stride, std::min(gpuStream->stride(), localStream.m_stride));
            dest.WriteRaw(i * gpuStream->stride(), sp);
        }
        if (m_isSkinnedMesh) {
            for (size_t i = 0; i < m_numberVertices; i++) {
                auto sp = src.rawByteSpan().subspan(i * localStream.m_stride, std::min(gpuStream->stride(), localStream.m_stride));
                dest.WriteRaw(((i + m_numberVertices) * gpuStream->stride()), sp);
            }
        }
        endUpdateResource(&updateDesc);
    }

    void cSubMeshEntity::updateIndexStream(cSubMesh::IndexBufferInfo& stream) {
        BufferUpdateDesc updateDesc = { m_geometry->indexBuffer().m_handle,
                                        m_geometry->indexOffset() * GeometrySet::IndexBufferStride,
                                        GeometrySet::IndexBufferStride * m_numberIndecies };
        beginUpdateResource(&updateDesc);
        GraphicsBuffer gpuBuffer(updateDesc);
        auto dest = gpuBuffer.CreateIndexView();
        auto src = stream.m_buffer.CreateIndexView();
        for (size_t i = 0; i < m_numberIndecies; i++) {
            dest.Write(i, src.Get(i));
        }
        endUpdateResource(&updateDesc);
    }
    void cSubMeshEntity::rebuildGpuVertexStreams() {
        auto* graphicsAllocator = Interface<GraphicsAllocator>::Get();
        ASSERT(graphicsAllocator);
        auto& opaqueSet = graphicsAllocator->resolveSet(GraphicsAllocator::AllocationSet::OpaqueSet);

        auto vertexStreams = m_subMesh->streamBuffers();
        auto& indexStream = m_subMesh->IndexStream();
        m_numberVertices = vertexStreams.begin()->m_numberElements;
        m_numberIndecies = indexStream.m_numberElements;

        const uint32_t reservedVerticies = m_numberVertices * (m_isSkinnedMesh ? 2 : 1);
        m_geometry = opaqueSet.allocate(reservedVerticies, m_numberIndecies);
        for(auto& localStream: vertexStreams) {
            updateVertexStream(localStream);
        }
        updateIndexStream(indexStream);
    }

    bool cSubMeshEntity::isGeometryMismatch(cSubMesh* submesh) {
        if (submesh->IndexStream().m_numberElements != m_numberIndecies) {
            return true;
        }
        auto vertexStreams = m_subMesh->streamBuffers();
        for(auto& localStream: vertexStreams) {
            if(localStream.m_numberElements != m_numberVertices) {
                return true;
            }
        }
        return false;
    }

    cSubMeshEntity::cSubMeshEntity(
        const tString& asName, cMeshEntity* apMeshEntity, cSubMesh* apSubMesh, cMaterialManager* apMaterialManager)
        : iRenderable(asName)
        , mpMeshEntity(apMeshEntity)
        , m_subMesh(apSubMesh)
        , mpMaterialManager(apMaterialManager) {
        mbIsOneSided = m_subMesh->GetIsOneSided();
        mvOneSidedNormal = m_subMesh->GetOneSidedNormal();
        m_isSkinnedMesh  = (mpMeshEntity->GetMesh()->GetSkeleton() != nullptr);
        auto* graphicsAllocator = Interface<GraphicsAllocator>::Get();
        ASSERT(graphicsAllocator);
        auto& opaqueSet = graphicsAllocator->resolveSet(GraphicsAllocator::AllocationSet::OpaqueSet);
        m_subMeshChangeHandler = cSubMesh::NotifySubMeshChanged::Handler([&](cSubMesh::NotifyMesh notify) {
            if (isGeometryMismatch(m_subMesh)) {
                rebuildGpuVertexStreams();
                return;
            }
            if (notify.changeIndexData) {
                updateIndexStream(m_subMesh->IndexStream());
            }
            for (size_t i = 0; i < notify.m_semanticSize; i++) {
                updateVertexStream(*m_subMesh->getStreamBySemantic(notify.m_semantic[i]));
            }
        });
        m_subMeshChangeHandler.Connect(m_subMesh->m_notify);
        rebuildGpuVertexStreams();
    }

    cSubMeshEntity::~cSubMeshEntity() {
        /* Clear any custom textures here*/
        if (mpMaterial)
            mpMaterialManager->Destroy(mpMaterial);
    }

    void cSubMeshEntity::UpdateLogic(float afTimeStep) {
    }

    cMaterial* cSubMeshEntity::GetMaterial() {
        if (mpMaterial == NULL && m_subMesh->GetMaterial() == NULL) {
            // Error("Materials for sub entity %s are NULL!\n",GetName().c_str());
        }

        if (mpMaterial)
            return mpMaterial;
        else
            return m_subMesh->GetMaterial();
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
        // Update things in parent first.
        mpMeshEntity->UpdateGraphicsForFrame(afFrameTime);

        ////////////////////////////////////
        // If it has dynamic mesh, update it.
        if (m_isSkinnedMesh) {
            if (mpMeshEntity->mbSkeletonPhysicsSleeping && mbGraphicsUpdated) {
                return;
            }

            m_activeCopy = (m_activeCopy + 1) % ForgeRenderer::SwapChainLength;
            mbGraphicsUpdated = true;

            auto staticBindBuffers = m_subMesh->streamBuffers();
            auto bindPositionIt = m_subMesh->getStreamBySemantic(ShaderSemantic::SEMANTIC_POSITION);
            auto bindNormalIt = m_subMesh->getStreamBySemantic(ShaderSemantic::SEMANTIC_NORMAL);
            auto bindTangentIt = m_subMesh->getStreamBySemantic(ShaderSemantic::SEMANTIC_TANGENT);
            ASSERT(bindPositionIt != m_subMesh->streamBuffers().end() &&
                bindNormalIt != m_subMesh->streamBuffers().end()  &&
                bindTangentIt != m_subMesh->streamBuffers().end()
            );
            auto bindPositonView = bindPositionIt->GetStructuredView<float3>(); // the static buffer data is the fixed pose
            auto bindNormalView = bindNormalIt->GetStructuredView<float3>();
            auto bindTangentView = bindTangentIt->GetStructuredView<float3>();

            auto targetPositionIt = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_POSITION);
            auto targetNormalIt  = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_NORMAL);
            auto targetTangentIt  = m_geometry->getStreamBySemantic(ShaderSemantic::SEMANTIC_TANGENT);

            ASSERT(targetPositionIt != m_geometry->vertexStreams().end() &&
                targetNormalIt != m_geometry->vertexStreams().end()  &&
                targetTangentIt != m_geometry->vertexStreams().end()
            );

            BufferUpdateDesc positionUpdateDesc = { targetPositionIt->buffer().m_handle, (m_geometry->vertexOffset() * targetPositionIt->stride()) + (m_activeCopy * targetPositionIt->stride() * m_numberVertices), targetPositionIt->stride() * m_numberVertices};
            BufferUpdateDesc tangentUpdateDesc = { targetTangentIt->buffer().m_handle, (m_geometry->vertexOffset() * targetTangentIt->stride()) + (m_activeCopy * targetTangentIt->stride() * m_numberVertices), targetTangentIt->stride() * m_numberVertices};
            BufferUpdateDesc normalUpdateDesc = { targetNormalIt->buffer().m_handle, (m_geometry->vertexOffset() * targetNormalIt->stride()) + (m_activeCopy * targetNormalIt->stride() * m_numberVertices), targetNormalIt->stride() * m_numberVertices};
            beginUpdateResource(&positionUpdateDesc);
            beginUpdateResource(&tangentUpdateDesc);
            beginUpdateResource(&normalUpdateDesc);

            GraphicsBuffer positionMapping(positionUpdateDesc);
            GraphicsBuffer normalMapping(normalUpdateDesc);
            GraphicsBuffer tangentMapping(tangentUpdateDesc);
            auto targetPositionView = positionMapping.CreateStructuredView<float3>(0, targetPositionIt->stride());
            auto targetTangentView = tangentMapping.CreateStructuredView<float3>(0, targetTangentIt->stride());
            auto targetNormalView = normalMapping.CreateStructuredView<float3>(0, targetNormalIt->stride());

            ASSERT(bindPositionIt->m_numberElements == m_numberVertices);
            for(size_t i = 0; i < m_numberVertices; i++) {
                const std::span<float> weights = std::span<float>(&m_subMesh->m_vertexWeights[(i * 4)], 4);
                const std::span<uint8_t> boneIdxs = std::span<uint8_t>(&m_subMesh->m_vertexBones[(i * 4)], 4);

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

            endUpdateResource(&positionUpdateDesc);
            endUpdateResource(&tangentUpdateDesc);
            endUpdateResource(&normalUpdateDesc);
		}

	}


    DrawPacket cSubMeshEntity::ResolveDrawPacket(const ForgeRenderer::Frame& frame)  {
		DrawPacket packet;
	    if(m_numberIndecies == 0) {
            return packet;
	    }

        DrawPacket::GeometrySetBinding binding{};
        packet.m_type = DrawPacket::DrawGeometryset;
        binding.m_subAllocation = m_geometry.get();
        binding.m_indexOffset = 0;
        binding.m_set = GraphicsAllocator::AllocationSet::OpaqueSet;
        binding.m_numIndices = m_numberIndecies;
	    binding.m_vertexOffset = (m_activeCopy * m_numberVertices);
        packet.m_unified = binding;
	    return packet;
    }
	iVertexBuffer* cSubMeshEntity::GetVertexBuffer()
	{
		return m_subMesh->GetVertexBuffer();
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
