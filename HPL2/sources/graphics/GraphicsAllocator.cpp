#include "graphics/GraphicsAllocator.h"
#include "graphics/ForgeRenderer.h"

namespace hpl {

    GraphicsAllocator::GraphicsAllocator(ForgeRenderer* renderer):
        m_renderer(renderer),
        m_vertexBufferAllocator(GraphicsAllocator::SharedVertexBufferSize),
        m_indexBufferAllocator(GraphicsAllocator::SharedIndexBufferSize) {
        m_sharedIndexBuffer.Load([&](Buffer** buffer) {
            BufferLoadDesc loadDesc = {};
            loadDesc.ppBuffer = buffer;
            loadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
            loadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
            loadDesc.mDesc.mSize = GraphicsAllocator::SharedIndexBufferSize; // will have a copy per frame
            addResource(&loadDesc, nullptr);
            return true;
        });
        m_sharedVertexBuffer.Load([&](Buffer** buffer) {
            BufferLoadDesc loadDesc = {};
            loadDesc.ppBuffer = buffer;
            loadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
            loadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
            loadDesc.mDesc.mSize = GraphicsAllocator::SharedVertexBufferSize; // will have a copy per frame
            addResource(&loadDesc, nullptr);
            return true;
        });
        {
            BufferDesc desc = {};
			desc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
            desc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
            desc.mSize = ImmediateVertexBufferSize;
            desc.pName = "Immediate Vertex Buffer";
			addGPURingBuffer(renderer->Rend(), &desc, &m_transientVertexBuffer);
	    }
        {
            BufferDesc desc = {};
		    desc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
            desc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
            desc.mSize = ImmediateIndexBufferSize;
            desc.pName = "Immediate Index Buffer";
			addGPURingBuffer(renderer->Rend(), &desc, &m_transientIndeciesBuffer);
	    }
    }

    GraphicsAllocator::OffsetAllocHandle::OffsetAllocHandle(OffsetAllocator::Allocator* allocator, OffsetAllocator::Allocation allocation ,SharedBuffer buffer):
        m_allocator(allocator),
        m_allocation(allocation),
        m_buffer(buffer) {
    }

    GraphicsAllocator::OffsetAllocHandle::~OffsetAllocHandle() {
        if(m_allocation.offset != OffsetAllocator::Allocation::NO_SPACE) {
            ASSERT(m_allocator != nullptr);
            m_allocator->free(m_allocation);
        }

    }
    GPURingBufferOffset GraphicsAllocator::allocTransientVertexBuffer(uint32_t size) {
		return getGPURingBufferOffset(m_transientVertexBuffer, size);
    }
    GPURingBufferOffset  GraphicsAllocator::allocTransientIndexBuffer(uint32_t size) {
		return getGPURingBufferOffset(m_transientIndeciesBuffer, size);
    }

    GraphicsAllocator::OffsetAllocHandle GraphicsAllocator::allocVertexFromSharedBuffer(uint32_t size) {
        OffsetAllocator::Allocation alloc = m_vertexBufferAllocator.allocate(size);
        return OffsetAllocHandle(&m_vertexBufferAllocator, alloc, m_sharedVertexBuffer);
    }
    GraphicsAllocator::OffsetAllocHandle  GraphicsAllocator::allocIndeciesFromSharedBuffer(uint32_t size) {
        OffsetAllocator::Allocation alloc = m_indexBufferAllocator.allocate(size);
        return OffsetAllocHandle(&m_indexBufferAllocator, alloc, m_sharedIndexBuffer);
    }
}
