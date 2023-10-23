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

#include "graphics/SubMesh.h"

#include "graphics/GraphicsBuffer.h"
#include "graphics/GraphicsTypes.h"
#include "impl/LegacyVertexBuffer.h"

#include "graphics/Bone.h"
#include "graphics/Material.h"
#include "graphics/Mesh.h"
#include "graphics/Skeleton.h"
#include "graphics/VertexBuffer.h"
#include "resources/MaterialManager.h"

#include "physics/PhysicsWorld.h"

#include "system/MemoryManager.h"
#include "system/String.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>

#include "math/Math.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Utilities/Math/MathTypes.h"
#include "FixPreprocessor.h"

namespace hpl {
    namespace detail {
        template<typename Trait>
        void TryCopyToStreamBuffers(LegacyVertexBuffer* buffer, eVertexBufferElement element, std::vector<cSubMesh::StreamBufferInfo>& vertexBuffers) {
            auto* lvbElement = buffer->GetElement(element);
            if(lvbElement) {
                cSubMesh::StreamBufferInfo& info = vertexBuffers.emplace_back();
                GraphicsBuffer::BufferStructuredView<typename Trait::Type> view;
                cSubMesh::StreamBufferInfo::InitializeBuffer<Trait>(&info, &view);
                uint32_t numElements = lvbElement->NumElements();
                view.ReserveElements(numElements);
                for(size_t i = 0; i < numElements; i++) {
                    auto value = lvbElement->GetElement<typename Trait::Type>(i);
                    view.Write(i, value);
                }
            }
        }

        void CopyStreamBufferVertexBuffer(cSubMesh::StreamBufferInfo& srcInfo, eVertexBufferElement element, uint32_t elementCount, LegacyVertexBuffer* target) {
            auto* lvbElement = target->GetElement(element);
            if(!lvbElement) {
                target->CreateElementArray(element, eVertexBufferElementFormat_Float, elementCount);
                lvbElement = target->GetElement(element);
            }
            ASSERT(lvbElement);
            ASSERT(lvbElement->m_format == eVertexBufferElementFormat::eVertexBufferElementFormat_Float);
            auto rawView = srcInfo.m_buffer.CreateViewRaw();
            target->ResizeArray(element, elementCount * srcInfo.m_numberElements);
            float* destData = target->GetFloatArray(element);
            for(size_t idx = 0; idx < srcInfo.m_numberElements; idx++) {
                auto buf = rawView.RawView();
                std::memcpy(destData + (elementCount * idx), buf.data() + (srcInfo.m_stride * idx), std::min(srcInfo.m_stride, static_cast<uint32_t>(sizeof(float) * elementCount)));
            }
        }
    }

    cSubMesh::cSubMesh(const tString& asName, cMaterialManager* apMaterialManager)
        : m_materialManager(apMaterialManager)
        , m_name(asName) {
    }

    cSubMesh::~cSubMesh() {
        if (m_material)
            m_materialManager->Destroy(m_material);
        if (m_vtxBuffer)
            hplDelete(m_vtxBuffer);
    }

    void cSubMesh::SetMaterial(cMaterial* apMaterial) {
        if (m_material)
            m_materialManager->Destroy(m_material);
        m_material = apMaterial;
    }

    void cSubMesh::SetVertexBuffer(iVertexBuffer* apVtxBuffer) {
        m_vtxBuffer = apVtxBuffer;
    }

    cMaterial* cSubMesh::GetMaterial() {
        return m_material;
    }

    iVertexBuffer* cSubMesh::GetVertexBuffer() {
        return m_vtxBuffer;
    }

    void cSubMesh::ResizeVertexBonePairs(int alSize) {
        m_vtxBonePairs.resize(alSize);
    }

    int cSubMesh::GetVertexBonePairNum() {
        return (int)m_vtxBonePairs.size();
    }

    cVertexBonePair& cSubMesh::GetVertexBonePair(int alNum) {
        return m_vtxBonePairs[alNum];
    }

    void cSubMesh::AddVertexBonePair(const cVertexBonePair& aPair) {
        m_vtxBonePairs.push_back(aPair);
    }

    void cSubMesh::ClearVertexBonePairs() {
        m_vtxBonePairs.clear();
    }

    std::span<cSubMesh::MeshCollisionResource> cSubMesh::GetColliders() {
        return m_colliders;
    }

    void cSubMesh::AddCollider(const MeshCollisionResource& def) {
        m_colliders.push_back(def);
    }

    void cSubMesh::WriteToVertexBuffer(std::vector<StreamBufferInfo>& vertexBuffers, IndexBufferInfo& indexBuffer, iVertexBuffer* output) {
        ASSERT(output);
        LegacyVertexBuffer* target= static_cast<LegacyVertexBuffer*>(output);
        {
            output->ResizeIndices(indexBuffer.m_numberElements);
            auto view = indexBuffer.GetView();
            uint32_t* target = output->GetIndices();
            for(size_t idx = 0; idx < indexBuffer.m_numberElements; idx++) {
                uint32_t vtxIdx = view.Get(idx);
                ASSERT(vtxIdx != UINT32_MAX);
                target[idx] = view.Get(idx);
            }
        }
        // this format is standard for how the engine creates vertex buffers same layout throughout the entire engine
        struct {
            ShaderSemantic srcSemantic;
            eVertexBufferElement destSemantic;
            uint32_t elementCount;
        } mapping[] = {
            {ShaderSemantic::SEMANTIC_POSITION, eVertexBufferElement_Position, 4},
            {ShaderSemantic::SEMANTIC_NORMAL, eVertexBufferElement_Normal, 3},
            {ShaderSemantic::SEMANTIC_COLOR, eVertexBufferElement_Color0, 4},
            {ShaderSemantic::SEMANTIC_TEXCOORD0, eVertexBufferElement_Texture0, 3},
            {ShaderSemantic::SEMANTIC_TANGENT, eVertexBufferElement_Texture1Tangent, 4}
        };

        for(auto& stream: vertexBuffers) {
            for(auto tp: mapping) {
                if(tp.srcSemantic == stream.m_semantic) {
                    detail::CopyStreamBufferVertexBuffer(stream, tp.destSemantic, tp.elementCount, target);
                    break;
                }
            }
        }
        output->Compile(0);
    }

    void cSubMesh::ReadFromVertexBuffer(iVertexBuffer* input, std::vector<StreamBufferInfo>& streamBuffers, IndexBufferInfo& indexBuffer) {
        ASSERT(input);
        LegacyVertexBuffer* lvb = static_cast<LegacyVertexBuffer*>(input);
        detail::TryCopyToStreamBuffers<PostionTrait>(lvb, eVertexBufferElement_Position, streamBuffers);
        detail::TryCopyToStreamBuffers<NormalTrait>(lvb, eVertexBufferElement_Normal, streamBuffers);
        detail::TryCopyToStreamBuffers<ColorTrait>(lvb, eVertexBufferElement_Color0, streamBuffers);
        detail::TryCopyToStreamBuffers<TangentTrait>(lvb, eVertexBufferElement_Texture1Tangent, streamBuffers);
        detail::TryCopyToStreamBuffers<TextureTrait>(lvb, eVertexBufferElement_Texture0, streamBuffers);
        {
            auto indexView = indexBuffer.GetView();
            auto* target = input->GetIndices();
            for(size_t i = 0; i < indexBuffer.m_numberElements; i++) {
                indexView.Write(i, target[i]);
            }
        }
    }
    void cSubMesh::SetStreamBuffers(iVertexBuffer* buffer, std::vector<StreamBufferInfo>&& vertexStreams, IndexBufferInfo&& indexStream) {
        m_vertexStreams = std::move(vertexStreams);
        m_indexStream = std::move(indexStream);

        if(buffer) {
            m_vtxBuffer = buffer;
            WriteToVertexBuffer(m_vertexStreams, m_indexStream, m_vtxBuffer);
        }
        auto posStream = std::find_if(m_vertexStreams.begin(), m_vertexStreams.end(), [&](auto& stream) {
            return stream.m_semantic == ShaderSemantic::SEMANTIC_POSITION;
        });
        if (m_indexStream.m_numberElements <= 400 * 3 && posStream != m_vertexStreams.end()) {
            auto index = m_indexStream.GetView();
            Vector3 normalSum = Vector3(0, 0, 0);
            Vector3 firstNormal = Vector3(0, 0, 0);
            Vector3 positionSum = Vector3(0, 0, 0);
            float triCount = 0;
            auto posView = posStream->GetStructuredView<float3>();
            for(size_t i = 0; i < m_indexStream.m_numberElements; i += 3) {
                uint32_t t1 = index.Get(i);
                uint32_t t2 = index.Get(i);
                uint32_t t3 = index.Get(i);
                const float3 pVtx0 = posView.Get(t1);
                const float3 pVtx1 = posView.Get(t2);
                const float3 pVtx2 = posView.Get(t3);

                Vector3 edge1(pVtx1[0] - pVtx0[0], pVtx1[1] - pVtx0[1], pVtx1[2] - pVtx0[2]);
                Vector3 edge2(pVtx2[0] - pVtx0[0], pVtx2[1] - pVtx0[1], pVtx2[2] - pVtx0[2]);
                Vector3 current = normalize(cross(edge2, edge1));

                positionSum += Vector3(pVtx0[0], pVtx0[1], pVtx0[2]);

                if (i == 0) {
                    firstNormal = current;
                    normalSum = current;
                } else {
                    float cosAngle = dot(firstNormal, current);
                    if (cosAngle < 0.9f) {
                        return;
                    }
                    normalSum += current;
                }
                triCount += 1;

            }
            m_isOneSided = true;
            m_oneSidedNormal = cMath::FromForgeVector3(normalize(normalSum / triCount));
            m_oneSidedPoint = cMath::FromForgeVector3(positionSum / triCount);
        }
    }
    iCollideShape* cSubMesh::CreateCollideShapeFromCollider(
        const MeshCollisionResource& pCollider, iPhysicsWorld* apWorld, const cVector3f& avSizeMul, const std::optional<cMatrixf> apMtxOffset) {
        cMatrixf pOffset = apMtxOffset.value_or(pCollider.m_mtxOffset);

        cVector3f vSize = pCollider.mvSize * avSizeMul;

        switch (pCollider.mType) {
        case eCollideShapeType_Box:
            return apWorld->CreateBoxShape(vSize, &pOffset);
        case eCollideShapeType_Sphere:
            return apWorld->CreateSphereShape(vSize, &pOffset);
        case eCollideShapeType_Cylinder:
            return apWorld->CreateCylinderShape(vSize.x, vSize.y, &pOffset);
        case eCollideShapeType_Capsule:
            return apWorld->CreateCapsuleShape(vSize.x, vSize.y, &pOffset);
        default:
            break;
        }

        return NULL;
    }

    iCollideShape* cSubMesh::CreateCollideShape(iPhysicsWorld* apWorld, std::span<MeshCollisionResource> colliders) {
        if (colliders.empty()) {
            return nullptr;
        }

        // Create a single object
        if (colliders.size() == 1) {
            return CreateCollideShapeFromCollider(colliders[0], apWorld, 1, std::nullopt);
        }
        std::vector<iCollideShape*> vShapes;
        vShapes.reserve(colliders.size());
        for (size_t i = 0; i < colliders.size(); ++i) {
            vShapes.push_back(CreateCollideShapeFromCollider(colliders[i], apWorld, 1, std::nullopt));
        }

        return apWorld->CreateCompundShape(vShapes);
    }

    //-----------------------------------------------------------------------

    void cSubMesh::Compile() {
        // build plan normal?
        if (m_vtxBuffer && m_vtxBuffer->GetIndexNum() <= 400 * 3) {
            [&] {
                unsigned int* pIndices = m_vtxBuffer->GetIndices();
                float* pPositions = m_vtxBuffer->GetFloatArray(eVertexBufferElement_Position);

                Vector3 normalSum = Vector3(0, 0, 0);
                Vector3 firstNormal = Vector3(0, 0, 0);
                Vector3 positionSum = Vector3(0, 0, 0);
                const int vertexStride = m_vtxBuffer->GetElementNum(eVertexBufferElement_Position);
                float triCount = 0;
                for (int i = 0; i < m_vtxBuffer->GetIndexNum(); i += 3) {
                    uint32_t t1 = pIndices[i + 0];
                    uint32_t t2 = pIndices[i + 1];
                    uint32_t t3 = pIndices[i + 2];

                    const float* pVtx0 = &pPositions[t1 * vertexStride];
                    const float* pVtx1 = &pPositions[t2 * vertexStride];
                    const float* pVtx2 = &pPositions[t3 * vertexStride];

                    Vector3 edge1(pVtx1[0] - pVtx0[0], pVtx1[1] - pVtx0[1], pVtx1[2] - pVtx0[2]);
                    Vector3 edge2(pVtx2[0] - pVtx0[0], pVtx2[1] - pVtx0[1], pVtx2[2] - pVtx0[2]);
                    Vector3 current = normalize(cross(edge2, edge1));

                    positionSum += Vector3(pVtx0[0], pVtx0[1], pVtx0[2]);

                    if (i == 0) {
                        firstNormal = current;
                        normalSum = current;
                    } else {
                        float cosAngle = dot(firstNormal, current);
                        if (cosAngle < 0.9f) {
                            return;
                        }
                        normalSum += current;
                    }
                    triCount += 1;
                }
                m_isOneSided = true;
                m_oneSidedNormal = cMath::FromForgeVector3(normalize(normalSum / triCount));
                m_oneSidedPoint = cMath::FromForgeVector3(positionSum / triCount);
            }();
        }

        if (!m_vtxBonePairs.empty()) {
            m_vertexWeights.resize(4 * m_vtxBuffer->GetVertexNum(), 0);
            m_vertexBones.resize(4 * m_vtxBuffer->GetVertexNum(), 0);
            for (size_t i = 0; i < m_vtxBonePairs.size(); i++) {
                cVertexBonePair& pair = m_vtxBonePairs[i];
                bool foundWeight = false;
                size_t j = 0;
                for (; j < 4; j++) {
                    if (m_vertexWeights[(pair.vtxIdx * 4) + j] == 0) {
                        foundWeight = true;
                        break;
                    }
                }
                if (!foundWeight) {
                    LOGF(
                        LogLevel::eWARNING,
                        "More than 4 bones on a vertex in submesh '%s' in mesh '%s' !\n",
                        GetName().c_str(),
                        m_parent->GetName().c_str());
                    continue;
                }
                m_vertexWeights[(pair.vtxIdx * 4) + j] = pair.weight;
                m_vertexBones[(pair.vtxIdx * 4) + j] = pair.boneIdx;
            }
            /////////////////////////////////
            // Normalize the weights
            for (int vtx = 0; vtx < m_vtxBuffer->GetVertexNum(); ++vtx) {
                float total = 0;
                size_t count = 0;
                for (; count < 4; count++) {
                    float weight = m_vertexWeights[(vtx * 4) + count];
                    if (weight == 0) {
                        break;
                    }
                    total += weight;
                }

                if (count == 0) {
                    LOGF(
                        LogLevel::eWARNING,
                        "Some vertices in sub mesh '%s' in mesh '%s' are not connected to a bone!\n",
                        GetName().c_str(),
                        m_parent->GetName().c_str());
                    continue;
                }

                for (size_t j = 0; j < count; j++) {
                    m_vertexWeights[(vtx * 4) + j] /= total;
                }
            }
        }
    }
} // namespace hpl
