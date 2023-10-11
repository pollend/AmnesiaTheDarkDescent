#pragma once

#include "graphics/ForgeHandles.h"
#include <folly/small_vector.h>

#include <algorithm>
#include <optional>
#include <array>
#include <span>
#include <cstdint>

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include <FixPreprocessor.h>

namespace hpl {

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


    struct DrawRequest {
        struct VertexStream {
            SharedBuffer element;
            uint64_t offset;
            uint32_t stride;
        };
        struct IndexStream {
            SharedBuffer element;
            uint64_t offset;
            uint32_t stride;
	        uint32_t m_indexType : 2;
        };

        uint32_t numVertices;
        uint32_t numIndicies;
        folly::small_vector<VertexStream, eVertexBufferElement_LastEnum> m_vertexElement; // elements are in the order they are requested
        IndexStream m_indexElement;
    };

    class IndexBufferStream {
	    /// Index type (32 or 16 bit)
	    uint32_t m_indexType : 2;

        SharedBuffer m_buffer;
    };

    class VertexStream {
    public:
        VertexStream() {

        }

        VertexStream(VertexStream& stream):
            m_offset(stream.m_offset),
            m_stride(stream.m_stride),
            m_semantic(stream.m_semantic),
            m_buffer(stream.m_buffer) {
        }

        VertexStream(ShaderSemantic semantic, SharedBuffer buffer, uint32_t offset, uint32_t stride, bool useShadow):
            m_offset(offset),
            m_stride(stride),
            m_semantic(semantic),
            m_buffer(buffer) {
        }

        ShaderSemantic Semantic() {
            return m_semantic;
        }

        uint32_t Stride() {
            return m_stride;
        }

        uint32_t Offset() {
            return m_offset;
        }

        SharedBuffer& Buffer() {
            return m_buffer;
        }
    private:
        uint32_t m_offset = 0;
        uint32_t m_stride = 0;
        ShaderSemantic m_semantic = SEMANTIC_UNDEFINED;
        SharedBuffer m_buffer;
    };

    template<typename TTrait>
    class FixedVertexStream : public VertexStream{

    };

    // standard geometry
    class Geometry {
    public:

        Geometry() {
        }

        struct GeometryDescriptor {
            uint32_t m_numVertices = 0;
            uint32_t m_numIndecies = 0;
            std::optional<FixedVertexStream<PostionTrait>> m_position;
            std::optional<FixedVertexStream<NormalTrait>> m_normal;
            std::optional<FixedVertexStream<TangentTrait>> m_tangent;
            std::optional<FixedVertexStream<TextureTrait>> m_uv0;
            std::span<VertexStream> m_custom;
        };
        Geometry(GeometryDescriptor* desc) {
        }

        VertexStream* resolve(ShaderSemantic semantic) {
            switch(semantic) {
                case PostionTrait::Semantic:
                    return &m_position;
                case NormalTrait::Semantic:
                    return &m_normal;
                case TangentTrait::Semantic:
                    return &m_tangent;
                case TextureTrait::Semantic:
                    return &m_uv0;
                default:
                    break;
            }
            for(auto& stream: m_custom) {
            }
            return nullptr;
        }

        std::span<VertexStream> Custom() {
            return m_custom;
        }
        FixedVertexStream<PostionTrait>& Position() {
            return m_position;
        }
        FixedVertexStream<NormalTrait>& Normal() {
            return m_normal;
        }
        FixedVertexStream<TangentTrait>& Tangent() {
            return m_tangent;
        }
        FixedVertexStream<TextureTrait>& UV0() {
            return m_uv0;
        }

    private:
        folly::small_vector<SharedBuffer, MAX_VERTEX_BINDINGS> m_vertexBuffers;
        SharedBuffer m_indexBuffer;

        uint32_t m_vertexReserve;
        uint32_t m_indexReserve;

	    /// Number of indices in the geometry
	    uint32_t m_indexCount;
	    /// Number of vertices in the geometry
	    uint32_t m_vertexCount;

        FixedVertexStream<PostionTrait> m_position;
        FixedVertexStream<NormalTrait> m_normal;
        FixedVertexStream<TangentTrait> m_tangent;
        FixedVertexStream<TextureTrait> m_uv0;
        folly::small_vector<VertexStream, 15> m_custom;
        uint32_t m_numberVerticies;
    };

}
