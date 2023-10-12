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

#include "system/String.h"

#include "graphics/Bone.h"
#include "graphics/Material.h"
#include "graphics/Mesh.h"
#include "graphics/Skeleton.h"
#include "graphics/VertexBuffer.h"
#include "math/Math.h"
#include "resources/MaterialManager.h"

#include "physics/PhysicsWorld.h"

#include "system/MemoryManager.h"

#include <cstring>

namespace hpl {

    cSubMesh::cSubMesh(const tString& asName, cMaterialManager* apMaterialManager)
        : m_materialManager(apMaterialManager)
        , m_name(asName) {
    }

    cSubMesh::~cSubMesh() {
        if (m_material)
            m_materialManager->Destroy(m_material);
        if (m_vtxBuffer)
            hplDelete(m_vtxBuffer);
        STLDeleteAll(m_colliders);
    }

    void cSubMesh::SetMaterial(cMaterial* apMaterial) {
        if (m_material)
            m_materialManager->Destroy(m_material);
        m_material = apMaterial;
    }

    void cSubMesh::SetVertexBuffer(iVertexBuffer* apVtxBuffer) {
        if (m_vtxBuffer == apVtxBuffer)
            return;

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

    cMeshCollider* cSubMesh::CreateCollider(eCollideShapeType aType) {
        cMeshCollider* pColl = hplNew(cMeshCollider, ());
        pColl->mType = aType;

        m_colliders.push_back(pColl);

        return pColl;
    }

    cMeshCollider* cSubMesh::GetCollider(int alIdx) {
        return m_colliders[alIdx];
    }

    int cSubMesh::GetColliderNum() {
        return (int)m_colliders.size();
    }

    iCollideShape* cSubMesh::CreateCollideShapeFromCollider(
        cMeshCollider* pCollider, iPhysicsWorld* apWorld, const cVector3f& avSizeMul, cMatrixf* apMtxOffset) {
        cMatrixf* pOffset = apMtxOffset ? apMtxOffset : &pCollider->m_mtxOffset;

        cVector3f vSize = pCollider->mvSize * avSizeMul;

        switch (pCollider->mType) {
        case eCollideShapeType_Box:
            return apWorld->CreateBoxShape(vSize, pOffset);
        case eCollideShapeType_Sphere:
            return apWorld->CreateSphereShape(vSize, pOffset);
        case eCollideShapeType_Cylinder:
            return apWorld->CreateCylinderShape(vSize.x, vSize.y, pOffset);
        case eCollideShapeType_Capsule:
            return apWorld->CreateCapsuleShape(vSize.x, vSize.y, pOffset);
        }

        return NULL;
    }

    iCollideShape* cSubMesh::CreateCollideShape(iPhysicsWorld* apWorld) {
        if (m_colliders.empty()) {
            return nullptr;
        }

        // Create a single object
        if (m_colliders.size() == 1) {
            return CreateCollideShapeFromCollider(m_colliders[0], apWorld, 1, NULL);
        }
        std::vector<iCollideShape*> vShapes;
        vShapes.reserve(m_colliders.size());
        for (size_t i = 0; i < m_colliders.size(); ++i) {
            vShapes.push_back(CreateCollideShapeFromCollider(m_colliders[i], apWorld, 1, NULL));
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
