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

#pragma once

#include "graphics/GraphicsTypes.h"
#include "math/MathTypes.h"
#include "math/MeshTypes.h"
#include "physics/PhysicsTypes.h"
#include "system/SystemTypes.h"

namespace hpl {

    class cMaterial;
    class iVertexBuffer;

    class cMesh;
    class iPhysicsWorld;
    class iCollideShape;

    class cMaterialManager;

    class cMeshCollider {
    public:
        tString msGroup; // Only used as temp var when loading!

        eCollideShapeType mType;
        cVector3f mvSize;
        cMatrixf m_mtxOffset;
        bool mbCharCollider;
    };

    class cSubMesh final {
        friend class cMesh;
        friend class cSubMeshEntity;

    public:
        cSubMesh(const tString& asName, cMaterialManager* apMaterialManager);
        ~cSubMesh();

        void SetMaterial(cMaterial* apMaterial);
        void SetVertexBuffer(iVertexBuffer* apVtxBuffer);

        // Renderable implementation.
        cMaterial* GetMaterial();
        iVertexBuffer* GetVertexBuffer();

        const tString& GetName() {
            return m_name;
        }

        // Vertex-Bone pairs
        void ResizeVertexBonePairs(int alSize);
        int GetVertexBonePairNum();
        cVertexBonePair& GetVertexBonePair(int alNum);

        void AddVertexBonePair(const cVertexBonePair& aPair);
        void ClearVertexBonePairs();

        // Colliders
        cMeshCollider* CreateCollider(eCollideShapeType aType);
        cMeshCollider* GetCollider(int alIdx);
        int GetColliderNum();
        iCollideShape* CreateCollideShape(iPhysicsWorld* apWorld);
        static iCollideShape* CreateCollideShapeFromCollider(
            cMeshCollider* pCollider, iPhysicsWorld* apWorld, const cVector3f& avSizeMul, cMatrixf* apMtxOffset);

        void SetIsCollideShape(bool abX) {m_collideShape = abX; }
        bool IsCollideShape() { return m_collideShape; }

        void SetDoubleSided(bool abX) {m_doubleSided = abX; }
        bool GetDoubleSided() { return m_doubleSided; }

        inline void SetModelScale(const cVector3f& avScale) { m_modelScale = avScale; }
        inline cVector3f GetModelScale() { return m_modelScale; }

        const cMatrixf& GetLocalTransform() { return m_mtxLocalTransform; }
        void SetLocalTransform(const cMatrixf& a_mtxTrans) { m_mtxLocalTransform = a_mtxTrans; }

        bool GetIsOneSided() { return m_isOneSided; }
        const cVector3f& GetOneSidedNormal() { return m_oneSidedNormal; }
        const cVector3f& GetOneSidedPoint() { return m_oneSidedPoint; }
        void SetMaterialName(const tString& asName) { m_materialName = asName; }
        const tString& GetMaterialName() { return m_materialName; }

        void Compile();

    private:
        cMaterialManager* m_materialManager = nullptr;
        tString m_name;

        tString m_materialName;
        cMaterial* m_material = nullptr;
        iVertexBuffer* m_vtxBuffer = nullptr;

        cMatrixf m_mtxLocalTransform = cMatrixf::Identity;

        std::vector<cVertexBonePair> m_vtxBonePairs;
        std::vector<cMeshCollider*> m_colliders;

        std::vector<float> m_vertexWeights;
        std::vector<uint8_t> m_vertexBones;

        cVector3f m_modelScale;
        bool m_doubleSided = false;
        bool m_collideShape = false;
        bool m_isOneSided = false;
        cVector3f m_oneSidedNormal = 0;
        cVector3f m_oneSidedPoint = 0;

        cMesh* m_parent;
    };

}; // namespace hpl
