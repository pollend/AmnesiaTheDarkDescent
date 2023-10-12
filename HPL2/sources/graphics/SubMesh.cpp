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

    cSubMesh::cSubMesh(const tString& asName, cMaterialManager* apMaterialManager) {
        mpMaterialManager = apMaterialManager;

        msName = asName;

        mpMaterial = NULL;
        mpVtxBuffer = NULL;

        mbDoubleSided = false;

        mbCollideShape = false;

        m_mtxLocalTransform = cMatrixf::Identity;

        mbIsOneSided = false;
        mvOneSidedNormal = 0;
        mvOneSidedPoint = 0;
    }

    //-----------------------------------------------------------------------

    cSubMesh::~cSubMesh() {
        if (mpMaterial)
            mpMaterialManager->Destroy(mpMaterial);
        if (mpVtxBuffer)
            hplDelete(mpVtxBuffer);
        STLDeleteAll(mvColliders);
    }

    void cSubMesh::SetMaterial(cMaterial* apMaterial) {
        if (mpMaterial)
            mpMaterialManager->Destroy(mpMaterial);
        mpMaterial = apMaterial;
    }

    //-----------------------------------------------------------------------

    void cSubMesh::SetVertexBuffer(iVertexBuffer* apVtxBuffer) {
        if (mpVtxBuffer == apVtxBuffer)
            return;

        mpVtxBuffer = apVtxBuffer;
    }

    //-----------------------------------------------------------------------

    cMaterial* cSubMesh::GetMaterial() {
        return mpMaterial;
    }

    //-----------------------------------------------------------------------

    iVertexBuffer* cSubMesh::GetVertexBuffer() {
        return mpVtxBuffer;
    }

    //-----------------------------------------------------------------------

    void cSubMesh::ResizeVertexBonePairs(int alSize) {
        mvVtxBonePairs.resize(alSize);
    }

    int cSubMesh::GetVertexBonePairNum() {
        return (int)mvVtxBonePairs.size();
    }
    cVertexBonePair& cSubMesh::GetVertexBonePair(int alNum) {
        return mvVtxBonePairs[alNum];
    }

    void cSubMesh::AddVertexBonePair(const cVertexBonePair& aPair) {
        mvVtxBonePairs.push_back(aPair);
    }

    void cSubMesh::ClearVertexBonePairs() {
        mvVtxBonePairs.clear();
    }

    cMeshCollider* cSubMesh::CreateCollider(eCollideShapeType aType) {
        cMeshCollider* pColl = hplNew(cMeshCollider, ());
        pColl->mType = aType;

        mvColliders.push_back(pColl);

        return pColl;
    }

    cMeshCollider* cSubMesh::GetCollider(int alIdx) {
        return mvColliders[alIdx];
    }

    int cSubMesh::GetColliderNum() {
        return (int)mvColliders.size();
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
        if (mvColliders.empty()) {
            return nullptr;
        }

        // Create a single object
        if (mvColliders.size() == 1) {
            return CreateCollideShapeFromCollider(mvColliders[0], apWorld, 1, NULL);
        }
        std::vector<iCollideShape*> vShapes;
        vShapes.reserve(mvColliders.size());
        for (size_t i = 0; i < mvColliders.size(); ++i) {
            vShapes.push_back(CreateCollideShapeFromCollider(mvColliders[i], apWorld, 1, NULL));
        }

        return apWorld->CreateCompundShape(vShapes);
    }

    //-----------------------------------------------------------------------

    void cSubMesh::Compile() {
        // build plan normal?
        if (mpVtxBuffer && mpVtxBuffer->GetIndexNum() <= 400 * 3) {
            unsigned int* pIndices = mpVtxBuffer->GetIndices();
            float* pPositions = mpVtxBuffer->GetFloatArray(eVertexBufferElement_Position);

            Vector3 normalSum = Vector3(0, 0, 0);
            Vector3 firstNormal = Vector3(0, 0, 0);
            Vector3 positionSum = Vector3(0, 0, 0);
            const int vertexStride = mpVtxBuffer->GetElementNum(eVertexBufferElement_Position);
            float triCount = 0;

            for (int i = 0; i < mpVtxBuffer->GetIndexNum(); i += 3) {
                uint32_t t1 = pIndices[i + 0];
                uint32_t t2 = pIndices[i + 1];
                uint32_t t3 = pIndices[i + 2];

                const float* pVtx0 = &pPositions[t1 * vertexStride];
                const float* pVtx1 = &pPositions[t2 * vertexStride];
                const float* pVtx2 = &pPositions[t3 * vertexStride];

                Vector3 vEdge1(pVtx1[0] - pVtx0[0], pVtx1[1] - pVtx0[1], pVtx1[2] - pVtx0[2]);
                Vector3 vEdge2(pVtx2[0] - pVtx0[0], pVtx2[1] - pVtx0[1], pVtx2[2] - pVtx0[2]);
                Vector3 vNormal = normalize(cross(vEdge2, vEdge1));

                positionSum += Vector3(pVtx0[0], pVtx0[1], pVtx0[2]);

                if (i == 0) {
                    firstNormal = vNormal;
                    normalSum = vNormal;
                } else {
                    if (length(cross(firstNormal, vNormal)) < 0.9f) {
                        return;
                    }
                    normalSum += vNormal;
                }

                triCount += 1;
            }
            mbIsOneSided = true;
            mvOneSidedNormal = cMath::FromForgeVector3(normalize(normalSum / triCount));
            mvOneSidedPoint = cMath::FromForgeVector3(positionSum / triCount);
        }

        if (!mvVtxBonePairs.empty()) {
            m_vertexWeights.resize(4 * mpVtxBuffer->GetVertexNum(), 0);
            m_vertexBones.resize(4 * mpVtxBuffer->GetVertexNum(), 0);
            for (size_t i = 0; i < mvVtxBonePairs.size(); i++) {
                cVertexBonePair& pair = mvVtxBonePairs[i];
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
                        mpParent->GetName().c_str());
                    continue;
                }
                m_vertexWeights[(pair.vtxIdx * 4) + j] = pair.weight;
                m_vertexBones[(pair.vtxIdx * 4) + j] = pair.boneIdx;
            }
            /////////////////////////////////
            // Normalize the weights
            for (int vtx = 0; vtx < mpVtxBuffer->GetVertexNum(); ++vtx) {
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
                        mpParent->GetName().c_str());
                    continue;
                }

                for (size_t j = 0; j < count; j++) {
                    m_vertexWeights[(vtx * 4) + j] /= total;
                }
            }
        }
    }
} // namespace hpl
