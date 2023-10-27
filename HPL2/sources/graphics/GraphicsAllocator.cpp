#include "graphics/GraphicsAllocator.h"
#include "graphics/ForgeRenderer.h"
#include <memory>

namespace hpl {


    std::shared_ptr<GeometrySet::GeometrySetSubAllocation> GeometrySet::allocate(uint32_t numElements, uint32_t numIndecies) {
        auto subAllocation = std::make_shared<GeometrySet::GeometrySetSubAllocation>();
        subAllocation->m_indexAllocation = m_indexStreamAllocator.allocate(numIndecies);
        subAllocation->m_vertexAllocation = m_vertexStreamAllocator.allocate(numElements);
        subAllocation->m_geometrySet = this;
        return subAllocation;
    }

    GeometrySet::GeometryStream::GeometryStream(GeometryStream&& stream)
        : m_semantic(stream.m_semantic)
        , m_stride(stream.m_stride)
        , m_buffer(std::move(stream.m_buffer)) {
    }

    void GeometrySet::GeometryStream::operator=(GeometryStream&& stream){
        m_semantic = stream.m_semantic;
        m_stride = stream.m_stride;
        m_buffer = std::move(stream.m_buffer);
    }

    GeometrySet::GeometrySet(uint32_t numElements, uint32_t numIndecies, std::span<GeometryStreamDesc> streamDesc)
        : m_vertexStreamAllocator(numElements)
        , m_indexStreamAllocator(numIndecies) {
        for (auto& desc : streamDesc) {
            auto& vertexStream = m_vertexStreams.emplace_back();
            vertexStream.m_stride = desc.m_stride;
            vertexStream.m_semantic = desc.m_semantic;
            vertexStream.m_buffer.Load([&](Buffer** buffer) {
                BufferLoadDesc loadDesc = {};
                loadDesc.ppBuffer = buffer;
                loadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
                loadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
                loadDesc.mDesc.mSize = numElements * vertexStream.m_stride;
                loadDesc.mDesc.pName = desc.m_name;
                addResource(&loadDesc, nullptr);
                return true;
            });
        }
        m_indexBuffer.Load([&](Buffer** buffer) {
            BufferLoadDesc loadDesc = {};
            loadDesc.ppBuffer = buffer;
            loadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
            loadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
            loadDesc.mDesc.mSize = numIndecies * sizeof(uint32_t);
            loadDesc.mDesc.pName = "GeometrySet Index";
            addResource(&loadDesc, nullptr);
            return true;
        });
    }

    GeometrySet::GeometryStream::GeometryStream() {
    }

    GeometrySet::GeometrySet()
        : m_vertexStreamAllocator(0)
        , m_indexStreamAllocator(0) {
    }

    GeometrySet::GeometrySet(GeometrySet&& set)
        : m_vertexStreamAllocator(std::move(set.m_vertexStreamAllocator))
        , m_indexStreamAllocator(std::move(set.m_indexStreamAllocator))
        , m_vertexStreams(std::move(set.m_vertexStreams))
        , m_indexBuffer(std::move(set.m_indexBuffer)) {
    }

    void GeometrySet::operator=(GeometrySet&& set) {
        m_vertexStreamAllocator = std::move(set.m_vertexStreamAllocator);
        m_indexStreamAllocator = std::move(set.m_indexStreamAllocator);
        m_vertexStreams = std::move(set.m_vertexStreams);
        m_indexBuffer = std::move(set.m_indexBuffer);
    }

    GeometrySet& GraphicsAllocator::resolveSet(AllocationSet set) {
        return m_geometrySets[set];
    }

    GeometrySet::GeometrySetSubAllocation::GeometrySetSubAllocation(GeometrySetSubAllocation&& allocation):
        m_vertexAllocation(std::move(allocation.m_vertexAllocation)),
        m_indexAllocation(std::move(allocation.m_vertexAllocation)),
        m_geometrySet(allocation.m_geometrySet){
    }

    GeometrySet::GeometrySetSubAllocation::~GeometrySetSubAllocation() {
        if(m_geometrySet) {
            m_geometrySet->m_vertexStreamAllocator.free(m_vertexAllocation);
            m_geometrySet->m_indexStreamAllocator.free(m_indexAllocation);
        }
    }

    GeometrySet::GeometrySetSubAllocation::GeometrySetSubAllocation() {
    }

    GraphicsAllocator::GraphicsAllocator(ForgeRenderer* renderer)
        : m_renderer(renderer) {
        {
            std::array streamDesc = {
                GeometrySet::GeometryStreamDesc("opaque_position", ShaderSemantic::SEMANTIC_POSITION, sizeof(float3)),
                GeometrySet::GeometryStreamDesc("opaque_tangent", ShaderSemantic::SEMANTIC_TANGENT, sizeof(float3)),
                GeometrySet::GeometryStreamDesc("opaque_normal", ShaderSemantic::SEMANTIC_NORMAL, sizeof(float3)),
                GeometrySet::GeometryStreamDesc("opaque_uv0", ShaderSemantic::SEMANTIC_TEXCOORD0, sizeof(float2)),
                GeometrySet::GeometryStreamDesc("opaque_color", ShaderSemantic::SEMANTIC_COLOR, sizeof(float4)),
            };
            m_geometrySets[AllocationSet::OpaqueSet] = GeometrySet(OpaqueVertexBufferSize, OpaqueIndexBufferSize, streamDesc);
        }

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
            desc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
            desc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
            desc.mSize = ImmediateIndexBufferSize;
            desc.pName = "Immediate Index Buffer";
            addGPURingBuffer(renderer->Rend(), &desc, &m_transientIndeciesBuffer);
        }
    }

    GPURingBufferOffset GraphicsAllocator::allocTransientVertexBuffer(uint32_t size) {
        return getGPURingBufferOffset(m_transientVertexBuffer, size);
    }
    GPURingBufferOffset GraphicsAllocator::allocTransientIndexBuffer(uint32_t size) {
        return getGPURingBufferOffset(m_transientIndeciesBuffer, size);
    }

    //    GraphicsAllocator::OffsetAllocHandle GraphicsAllocator::allocVertexFromSharedBuffer(uint32_t size) {
    //        OffsetAllocator::Allocation alloc = m_vertexBufferAllocator.allocate(size);
    //        return OffsetAllocHandle(&m_vertexBufferAllocator, alloc, m_sharedVertexBuffer);
    //    }
    //    GraphicsAllocator::OffsetAllocHandle  GraphicsAllocator::allocIndeciesFromSharedBuffer(uint32_t size) {
    //        OffsetAllocator::Allocation alloc = m_indexBufferAllocator.allocate(size);
    //        return OffsetAllocHandle(&m_indexBufferAllocator, alloc, m_sharedIndexBuffer);
    //    }
} // namespace hpl
