#include "graphics/GraphicsAllocator.h"

namespace hpl {

    GraphicsAllocator::GraphicsAllocator():
        m_vertexBufferAllocator(GraphicsAllocator::SharedVertexBufferSize),
        m_indexBufferAllocator(GraphicsAllocator::SharedVertexBufferSize) {

    }
    void GraphicsAllocator::allocTransientVertexBuffer(uint32_t size) {

    }
    void GraphicsAllocator::allocTransientIndexBuffer(uint32_t size) {

    }

}
