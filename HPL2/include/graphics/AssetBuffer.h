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

namespace hpl {

    enum class BufferType : uint8_t {
        VertexBuffer,
        IndexBuffer,
        RawBuffer,
        Structured
    };
    struct BufferInfo {
        BufferType m_type;
        union {
            struct {
                uint32_t m_num;
                uint32_t m_stride;
                ShaderSemantic m_semantic;
            } m_vertex;
            struct {
                uint32_t m_num;
                uint32_t m_indexType: 2;
            } m_index;
            struct {
                uint32_t m_num;
                uint32_t m_stride;
            } m_structured;
            struct {
                uint32_t m_numBytes;
                uint32_t m_stride;
            } m_raw;
        };
    };

    class BufferAsset final {
    public:
        struct m_vertex_tag {};
        struct m_index_tag {};

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

        BufferAsset() {}
        BufferAsset(const BufferAsset&);
        BufferAsset(BufferAsset&&);
        explicit BufferAsset(ShaderSemantic semantic, uint32_t stride, uint32_t numVerticies, m_vertex_tag tag);
        explicit BufferAsset(uint32_t indexType, uint32_t numIndecies, m_index_tag tag);
        explicit BufferAsset(const BufferInfo& info);

        // utility function to create buffers allows for consistancy for these types of buffers in the engine
        template<typename Trait>
        static BufferAsset CreateBuffer() {
            BufferAsset asset;
            asset.m_info.m_type = BufferType::VertexBuffer;
            asset.m_info.m_vertex.m_stride = Trait::Stride;
            asset.m_info.m_vertex.m_semantic = Trait::Semantic;
            return asset;
        }

        class BufferRawView {
        public:
            BufferRawView(BufferAsset* asset, uint32_t byteOffset) :
                m_asset(asset),
                m_byteOffset(byteOffset) {
                ASSERT(m_byteOffset <= m_asset->SizeInBytes());
            }
            void SetBytes(uint32_t byteOffset, std::span<uint8_t> data) {
                for(size_t i = 0; i < data.size(); i++) {
                    m_asset->m_buffer[m_byteOffset + byteOffset + i] = data[i];
                }
            }

            inline std::span<uint8_t> RawView() { return m_asset->m_buffer; }
            inline uint32_t NumBytes() { return  (m_asset->m_buffer.size() - m_byteOffset); }

        protected:
            BufferAsset* m_asset;
            uint32_t m_byteOffset;
        };

        template<typename T>
        struct BufferStructuredView: BufferRawView {
        public:
            BufferStructuredView(BufferAsset* asset, uint32_t byteOffset, uint32_t byteStride)
                : BufferRawView(asset, byteOffset)
                , m_byteStride(byteStride) {
                ASSERT(m_byteStride >= sizeof(T));
            }

            inline uint32_t NumElements() {
                return NumBytes() / m_byteStride;
            }

            void Set(uint32_t index, const T& value) {
                *reinterpret_cast<T*>(m_asset->m_buffer.data() + (m_byteOffset + (index * m_byteStride))) = value;
            }

            void Set(uint32_t index, const std::span<T> values) {
                for (auto& v : values) {
                    Set(index, v);
                }
            }

            T Get(uint32_t index) {
                return *reinterpret_cast<T*>(m_asset->m_buffer.data() + (m_byteOffset + (index * m_byteStride)));
            }

            std::span<T> View() {
                ASSERT(m_byteStride == sizeof(T));
                return std::span<T>(reinterpret_cast<T*>(m_asset->m_buffer.data() + m_byteOffset), NumBytes() / sizeof(T));
            }
        protected:
            uint32_t m_byteStride;
        };

        template<typename Type>
        constexpr BufferStructuredView<Type> CreateStructuredView(uint32_t byteOffset = 0, uint32_t byteStride = 0) {
            return BufferStructuredView<Type>(this, byteOffset, (byteStride == 0) ? Stride() : byteStride);
        }

        BufferRawView CreateViewRaw(uint32_t byteOffset = 0) {
            return BufferRawView(this, byteOffset);
        }

        inline const BufferInfo& Info() {
            return m_info;
        }

        uint32_t Stride();
        uint32_t SizeInBytes();
        uint32_t NumElements();
    private:
        std::vector<uint8_t> m_buffer;
        BufferInfo m_info;
    };


} // namespace hpl
