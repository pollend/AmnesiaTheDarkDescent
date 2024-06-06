#include "graphics/GraphicsAllocator.h"
#include "graphics/ForgeRenderer.h"
#include <memory>

namespace hpl {

    GeometrySet& GraphicsAllocator::resolveSet(AllocationSet set) {
        return m_geometrySets[set];
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
            std::array streamDesc = {
                GeometrySet::GeometryStreamDesc("opaque_position", ShaderSemantic::SEMANTIC_POSITION, sizeof(float3)),
                GeometrySet::GeometryStreamDesc("opaque_uv0", ShaderSemantic::SEMANTIC_TEXCOORD0, sizeof(float2)),
                GeometrySet::GeometryStreamDesc("opaque_color", ShaderSemantic::SEMANTIC_COLOR, sizeof(float4)),
            };
            m_geometrySets[AllocationSet::ParticleSet] = GeometrySet(ParticleVertexBufferSize, ParticleIndexBufferSize, streamDesc);
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
        return getGPURingBufferOffset(&m_transientVertexBuffer, size);
    }
    GPURingBufferOffset GraphicsAllocator::allocTransientIndexBuffer(uint32_t size) {
        return getGPURingBufferOffset(&m_transientIndeciesBuffer, size);
    }
} // namespace hpl
