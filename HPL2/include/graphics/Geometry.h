#pragma once
#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "graphics/ForgeHandles.h"
#include <cstdint>
#include <folly/small_vector.h>
namespace hpl {
    struct Geometry {
    public:
        static constexpr uint32_t MaxVertexBindings = 15;

        struct StreamBufferInfo {
            SharedBuffer m_buffer;
            ShaderSemantic m_semantic;
            uint64_t m_stride;
            uint64_t m_offset;
        };

        struct IndexBufferInfo {
            SharedBuffer m_buffer;
            IndexType m_IndexType;
            uint64_t m_offset;
        };

        uint32_t m_numIndices;
        folly::small_vector<StreamBufferInfo, MaxVertexBindings> m_vertexStreams;
        IndexBufferInfo m_indexStream;
    };
}
