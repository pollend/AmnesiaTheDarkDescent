#pragma once

#include "graphics/AssetBuffer.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Utilities/ThirdParty/OpenSource/ModifiedSonyMath/common.hpp"
#include "FixPreprocessor.h"
#include <cstdint>

namespace hpl {

    BufferAsset::BufferAsset(const BufferAsset& buffer):
        m_info(buffer.m_info),
        m_buffer(buffer.m_buffer) {
    }
    BufferAsset::BufferAsset(BufferAsset&& buffer):
        m_info(buffer.m_info),
        m_buffer(std::move(buffer.m_buffer)){
    }

    BufferAsset::BufferAsset(ShaderSemantic semantic, uint32_t stride, uint32_t numVerticies, m_vertex_tag tag) {
        m_info.m_type = BufferType::VertexBuffer;
        m_info.m_vertex.m_stride = stride;
        m_info.m_vertex.m_num = numVerticies;
        m_info.m_vertex.m_semantic = semantic;
        m_buffer.resize(SizeInBytes(), 0);
    }

    BufferAsset::BufferAsset(uint32_t indexType, uint32_t numIndecies, m_index_tag tag) {
        m_info.m_type = BufferType::IndexBuffer;
        m_info.m_index.m_num = numIndecies;
        m_info.m_index.m_indexType = indexType;
        m_buffer.resize(SizeInBytes(), 0);
    }

    BufferAsset::BufferAsset(const BufferInfo& info):
        m_info(info) {
        m_buffer.resize(SizeInBytes(), 0);
    }

    uint32_t BufferAsset::Stride() {
        switch (m_info.m_type) {
        case BufferType::VertexBuffer:
            return m_info.m_vertex.m_stride;
        case BufferType::Structured:
            return m_info.m_structured.m_stride;
        case BufferType::IndexBuffer:
            {
                if (m_info.m_index.m_indexType == INDEX_TYPE_UINT16) {
                    return sizeof(uint16_t);
                }
                return sizeof(uint32_t);
            }
        case BufferType::RawBuffer:
            return m_info.m_raw.m_stride;
        }
        return 0;
    }
    uint32_t BufferAsset::SizeInBytes() {
        switch (m_info.m_type) {
        case BufferType::VertexBuffer:
            return m_info.m_vertex.m_num * m_info.m_vertex.m_stride;
        case BufferType::Structured:
            return m_info.m_structured.m_num * m_info.m_structured.m_stride;
        case BufferType::IndexBuffer:
            {
                if (m_info.m_index.m_indexType == INDEX_TYPE_UINT16) {
                    return sizeof(uint16_t) * m_info.m_index.m_num;
                }
                return sizeof(uint32_t) * m_info.m_index.m_num;
            }
        case BufferType::RawBuffer:
            return m_info.m_raw.m_numBytes;
        }
        return 0;
    }
    uint32_t BufferAsset::NumElements() {
        switch (m_info.m_type) {
        case BufferType::VertexBuffer:
            return m_info.m_vertex.m_num;
        case BufferType::Structured:
            return m_info.m_structured.m_num;
        case BufferType::IndexBuffer:
            return m_info.m_index.m_num;
        case BufferType::RawBuffer:
            return m_info.m_raw.m_numBytes / m_info.m_raw.m_stride;
        }
        return 0;
    }

} // namespace hpl
