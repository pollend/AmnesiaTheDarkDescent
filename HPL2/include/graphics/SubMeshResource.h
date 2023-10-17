/// Copyright 2023 Michael Pollind
/// SPDX-License-Identifier: GPL-3.0
#pragma once


#include <memory>
#include <optional>
#include <span>

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Utilities/Math/MathTypes.h"
#include "FixPreprocessor.h"

#include "graphics/AssetBuffer.h"
#include "system/SystemTypes.h"


namespace hpl {
    class cMaterial;
    class iVertexBuffer;

    class cMesh;
    class iPhysicsWorld;
    class iCollideShape;

    class cMaterialManager;

    class SubMeshResource final {
    public:
        struct StreamBufferInfo {
        public:
            StreamBufferInfo() {
            }

            StreamBufferInfo(const StreamBufferInfo& other):
                m_buffer(other.m_buffer),
                m_semantic(other.m_semantic),
                m_stride(other.m_stride),
                m_numberElements(other.m_numberElements){
            }
            StreamBufferInfo(StreamBufferInfo&& other):
                m_buffer(std::move(other.m_buffer)),
                m_semantic(other.m_semantic),
                m_stride(other.m_stride),
                m_numberElements(other.m_numberElements){
            }

            void operator=(const StreamBufferInfo& other) {
                m_buffer = other.m_buffer;
                m_semantic = other.m_semantic;
                m_stride = other.m_stride;
                m_numberElements = other.m_numberElements;
            }
            void operator=(StreamBufferInfo&& other) {
                m_buffer = std::move(other.m_buffer);
                m_semantic = other.m_semantic;
                m_stride = other.m_stride;
                m_numberElements = other.m_numberElements;
            }

            // utility function to create buffers allows for consistancy for these types of buffers in the engine
            template<typename Trait>
            static void InitializeBuffer(StreamBufferInfo* info, AssetBuffer::BufferStructuredView<typename Trait::Type>* view = nullptr) {
                info->m_stride = Trait::Stride;
                info->m_semantic = Trait::Semantic;
                if(view) {
                    (*view) = info->GetStructuredView<Trait>();
                }
            }

            template<typename Trait>
            constexpr AssetBuffer::BufferStructuredView<typename Trait::Type> GetStructuredView(uint32_t byteOffset = 0) {
                return m_buffer.CreateStructuredView<typename Trait::Type>(byteOffset, Trait::Stride);
            }

            AssetBuffer m_buffer;
            uint32_t m_stride = 0;
            uint32_t m_numberElements = 0;
            ShaderSemantic m_semantic = ShaderSemantic::SEMANTIC_UNDEFINED;
        };

        struct IndexBufferInfo {
        public:
            IndexBufferInfo() {
            }

            AssetBuffer::BufferIndexView GetView() {
                return m_buffer.CreateIndexView();
            }

        private:
            AssetBuffer m_buffer;
        };
        // preset traits that are expected throughout the engine
        struct PostionTrait {
            using Type = float3;
            static constexpr uint32_t Stride = sizeof(float3);
            static constexpr ShaderSemantic Semantic = ShaderSemantic::SEMANTIC_POSITION;
        };
        struct NormalTrait {
            using Type = float3;
            static constexpr uint32_t Stride = sizeof(float3);
            static constexpr ShaderSemantic Semantic = ShaderSemantic::SEMANTIC_NORMAL;
        };
        struct ColorTrait {
            using Type = float4;
            static constexpr uint32_t Stride = sizeof(float4);
            static constexpr ShaderSemantic Semantic = ShaderSemantic::SEMANTIC_COLOR;
        };
        struct TangentTrait {
            using Type = float3;
            static constexpr uint32_t Stride = sizeof(float3);
            static constexpr ShaderSemantic Semantic = ShaderSemantic::SEMANTIC_TANGENT;
        };
        struct TextureTrait {
            using Type = float2;
            static constexpr uint32_t Stride = sizeof(float2);
            static constexpr ShaderSemantic Semantic = ShaderSemantic::SEMANTIC_TEXCOORD0;
        };

        class MeshCollisionResource {
        public:
            eCollideShapeType mType;
            cVector3f m_size;
            cMatrixf m_mtxOffset;
            bool mbCharCollider;
        };

        static iCollideShape* CreateCollideShapeFromCollider(
            const MeshCollisionResource& pCollider,
            iPhysicsWorld* apWorld,
            const cVector3f& avSizeMul,
            const std::optional<cMatrixf> apMtxOffset = std::nullopt);
        static iCollideShape* CreateCollideShape(iPhysicsWorld* apWorld, std::span<MeshCollisionResource> resource);

       SubMeshResource(const tString& asName, cMaterialManager* apMaterialManager);

       ~SubMeshResource();
       SubMeshResource(SubMeshResource&& resource);
       SubMeshResource(const SubMeshResource& resource) = delete;

       void operator=(SubMeshResource&& resource);
       void operator=(SubMeshResource& resource) = delete;

       std::span<cVertexBonePair> GetVertexBonePairs();
       std::span<MeshCollisionResource> GetColliders();
       std::span<StreamBufferInfo> GetStreamBuffers();
       void SetMaterial(cMaterial* apMaterial);

        void SetIsCollideShape(bool abX) { m_collideShape = abX;}
        bool IsCollideShape() { return m_collideShape; }

        void SetDoubleSided(bool abX) { m_doubleSided = abX;}
        bool GetDoubleSided() { return m_doubleSided; }

        inline void SetModelScale(const Vector3& avScale) { m_modelScale = avScale; }
        inline Vector3 GetModelScale() { return m_modelScale; }

        const Matrix4& GetLocalTransform() { return m_mtxLocalTransform; }
        void SetLocalTransform(const Matrix4& a_mtxTrans) { m_mtxLocalTransform = a_mtxTrans; }

        bool GetIsOneSided() { return m_isOneSided; }
        const Vector3 GetOneSidedNormal() { return m_oneSidedNormal; }
        const Vector3 GetOneSidedPoint() { return m_oneSidedPoint; }
        void SetMaterialName(const tString& asName) { m_materialName = asName; }
        const tString& GetMaterialName() { return m_materialName; }
        const tString& GetName() {return m_name; }

        void SetVertexBonePairs(const std::vector<cVertexBonePair>& pair);
        void SetVertexBonePairs(const std::vector<cVertexBonePair>&& pair);

        void SetStreamBuffer(const std::vector<StreamBufferInfo>& info);
        void SetStreamBuffer(std::vector<StreamBufferInfo>&& info);

        void SetIndexBuffer(IndexBufferInfo&& info);
        void SetIndexBuffer(IndexBufferInfo& info);

        void SetCollisionResource(const std::vector<MeshCollisionResource>& info);
        void SetCollisionResource(std::vector<MeshCollisionResource>&& info);
    private:
        cMaterialManager* m_materialManager = nullptr;
        tString m_name;

        tString m_materialName;
        cMaterial* m_material = nullptr;

        Matrix4 m_mtxLocalTransform = Matrix4::identity();

        std::vector<cVertexBonePair> m_vtxBonePairs;
        std::vector<MeshCollisionResource> m_colliders;
        std::vector<StreamBufferInfo> m_vertexStreams;
        IndexBufferInfo m_index;
        std::vector<float> m_vertexWeights;
        std::vector<uint8_t> m_vertexBones;

        bool m_doubleSided = false;
        bool m_collideShape = false;
        bool m_isOneSided = false;
        Vector3 m_oneSidedNormal;
        Vector3 m_modelScale;
        Vector3 m_oneSidedPoint;

    };
} // namespace hpl
