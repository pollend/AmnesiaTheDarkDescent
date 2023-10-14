#pragma once

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "graphics/GraphicsTypes.h"
#include "math/MathTypes.h"
#include "math/MeshTypes.h"
#include "physics/PhysicsTypes.h"
#include "system/SystemTypes.h"

#include <optional>
#include <span>

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "Common_3/Utilities/Math/MathTypes.h"
#include "FixPreprocessor.h"

#include "graphics/AssetBuffer.h"

namespace hpl {
    class cMaterial;
    class iVertexBuffer;

    class cMesh;
    class iPhysicsWorld;
    class iCollideShape;

    class cMaterialManager;


    class SubMeshResource final {
        class MeshCollisionResource {
        public:
            eCollideShapeType mType;
            cVector3f mvSize;
            cMatrixf m_mtxOffset;
            bool mbCharCollider;
        };

        struct StreamBufferInfo {
        public:


        private:
            ShaderSemantic m_semantic;
            uint32_t m_stride;
            std::vector<uint8_t> m_buffer;
        };

    private:
        cMaterialManager* m_materialManager = nullptr;
        tString m_name;

        tString m_materialName;
        cMaterial* m_material = nullptr;
        iVertexBuffer* m_vtxBuffer = nullptr;

        cMatrixf m_mtxLocalTransform = cMatrixf::Identity;

        std::vector<cVertexBonePair> m_vtxBonePairs;
        std::vector<MeshCollisionResource> m_colliders;

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
} // namespace hpl
