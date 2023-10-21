
#pragma once

#include "graphics/AssetBuffer.h"
#include <cstdint>
#include <functional>

#include "Common_3/Utilities/Math/MathTypes.h"
#include "FixPreprocessor.h"

namespace hpl::MeshUtility {
    void Transform(
        uint32_t numElements,
        const Matrix4& transform,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferStructuredView<float3>* tangent
    );

    void MikktSpaceGenerate(
        uint32_t numVerts,
        uint32_t numIndecies,
        AssetBuffer::BufferIndexView* view,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float2>* uv,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferStructuredView<float3>* writeTangent
    );
}


