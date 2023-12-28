#pragma once

#include "graphics/ForgeHandles.h"
#include "graphics/ForgeRenderer.h"
#include "graphics/offsetAllocator.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Utilities/RingBuffer.h"
#include <folly/small_vector.h>

namespace hpl {

    class GeometrySet final {
    public:
        static constexpr uint32_t IndexBufferStride = sizeof(uint32_t);


        struct GeometryStreamDesc {
            const char* m_name;
            ShaderSemantic m_semantic;
            uint32_t m_stride;
        };
        class GeometryStream final {
        public:
            GeometryStream();
            GeometryStream(GeometryStream&&);
            void operator=(GeometryStream&& stream);

            GeometryStream(const GeometryStream&) = delete;
            void operator=(GeometryStream& stream) = delete;

            inline ShaderSemantic semantic() { return m_semantic; }
            inline uint32_t stride() { return m_stride; }
            inline SharedBuffer& buffer() { return m_buffer; }
        private:
            ShaderSemantic m_semantic = ShaderSemantic::SEMANTIC_UNDEFINED;
            uint32_t m_stride = 0;
            SharedBuffer m_buffer;
            friend class GeometrySet;
        };

        class GeometrySetSubAllocation final {
        public:
            ~GeometrySetSubAllocation();
            GeometrySetSubAllocation();
            GeometrySetSubAllocation(GeometrySetSubAllocation&& allocation);
            GeometrySetSubAllocation(const GeometrySetSubAllocation& allocation) = delete;

            void operator=(GeometrySetSubAllocation&& allocation);
            void operator=(const GeometrySetSubAllocation& allocation) = delete;

            inline SharedBuffer& indexBuffer() { return m_geometrySet->m_indexBuffer; }
            inline std::span<GeometryStream> vertexStreams() { return m_geometrySet->m_vertexStreams; }
            inline std::span<GeometryStream>::iterator getStreamBySemantic(ShaderSemantic semantic) {return m_geometrySet->getStreamBySemantic(semantic);}
            inline uint32_t vertexOffset() { return m_vertexAllocation.offset; }
            inline uint32_t indexOffset() { return m_indexAllocation.offset; }

        private:
            OffsetAllocator::Allocation m_vertexAllocation;
            OffsetAllocator::Allocation m_indexAllocation;
            GeometrySet* m_geometrySet = nullptr;
            friend class GeometrySet;
        };

        GeometrySet(GeometrySet&& set);
        GeometrySet(const GeometrySet& set) = delete;
        GeometrySet();
        explicit GeometrySet(uint32_t numElements, uint32_t numIndecies, std::span<GeometryStreamDesc> stream);
        void cmdBindGeometrySet(Cmd* cmd, std::span<ShaderSemantic> semantics);

        inline std::span<GeometryStream> vertexStreams() { return m_vertexStreams; }
        inline std::span<GeometryStream>::iterator getStreamBySemantic(ShaderSemantic semantic) {
            return std::find_if(vertexStreams().begin(), vertexStreams().end(), [&](auto& stream) {
                return stream.m_semantic == semantic;
            });
        }
        inline SharedBuffer& indexBuffer() { return m_indexBuffer; }
        void operator=(GeometrySet&& set);
        void operator=(const GeometrySet& set) = delete;
        std::shared_ptr<GeometrySet::GeometrySetSubAllocation> allocate(uint32_t numElements, uint32_t numIndecies);
    private:
        OffsetAllocator::Allocator m_vertexStreamAllocator;
        OffsetAllocator::Allocator m_indexStreamAllocator;
        folly::small_vector<GeometryStream, 15> m_vertexStreams;
        SharedBuffer m_indexBuffer;
        friend class GeometryStream;
    };
}
