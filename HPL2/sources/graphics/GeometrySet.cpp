#include "graphics/GeometrySet.h"
#include "graphics/offsetAllocator.h"


namespace hpl {

    void GeometrySet::cmdBindGeometrySet(Cmd* cmd, std::span<ShaderSemantic> semantics) {
        folly::small_vector<Buffer*, 16> bufferArgs;
        folly::small_vector<uint64_t, 16> offsetArgs;
        folly::small_vector<uint32_t, 16> strideArgs;
        for(auto& semantic: semantics) {
            auto stream = getStreamBySemantic(semantic);
            bufferArgs.push_back(stream->buffer().m_handle);
            offsetArgs.push_back(0);
            strideArgs.push_back(stream->stride());
        }
        cmdBindVertexBuffer(cmd, bufferArgs.size(), bufferArgs.data(), strideArgs.data(), offsetArgs.data());
        cmdBindIndexBuffer(cmd, indexBuffer().m_handle, INDEX_TYPE_UINT32, 0);
    }

    std::shared_ptr<GeometrySet::GeometrySetSubAllocation> GeometrySet::allocate(uint32_t numElements, uint32_t numIndecies) {
        auto subAllocation = std::make_shared<GeometrySet::GeometrySetSubAllocation>();
        subAllocation->m_indexAllocation = m_indexStreamAllocator.allocate(numIndecies);
        subAllocation->m_vertexAllocation = m_vertexStreamAllocator.allocate(numElements);

        auto vertexStorageReport = m_vertexStreamAllocator.storageReport();
        auto indexStorageReport = m_indexStreamAllocator.storageReport();
        //LOGF(LogLevel::eDEBUG, "vertex storage total free space: %d largest region: %d", vertexStorageReport.totalFreeSpace, vertexStorageReport.largestFreeRegion);
        //LOGF(LogLevel::eDEBUG, "index storage total free space: %d largest region: %d", indexStorageReport.totalFreeSpace, indexStorageReport.largestFreeRegion);

        ASSERT(subAllocation->m_indexAllocation.offset != OffsetAllocator::Allocation::NO_SPACE);
        ASSERT(subAllocation->m_indexAllocation.metadata != OffsetAllocator::Allocation::NO_SPACE);
        ASSERT(subAllocation->m_vertexAllocation.offset != OffsetAllocator::Allocation::NO_SPACE);
        ASSERT(subAllocation->m_vertexAllocation.metadata != OffsetAllocator::Allocation::NO_SPACE);

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
                loadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_BUFFER_RAW;
                loadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
                loadDesc.mDesc.mStructStride = vertexStream.m_stride;
                loadDesc.mDesc.mElementCount = numElements;
                loadDesc.mDesc.mSize = numElements * vertexStream.m_stride;
                loadDesc.mDesc.pName = desc.m_name;
                addResource(&loadDesc, nullptr);
                return true;
            });
        }
        m_indexBuffer.Load([&](Buffer** buffer) {
            BufferLoadDesc loadDesc = {};
            loadDesc.ppBuffer = buffer;
            loadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER | DESCRIPTOR_TYPE_BUFFER_RAW;
            loadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
            loadDesc.mDesc.mStructStride = sizeof(uint32_t);
            loadDesc.mDesc.mElementCount = numIndecies;
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

}
