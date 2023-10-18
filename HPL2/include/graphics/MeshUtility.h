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

    void CreateCapsule(
        float radius,
        float height,
        uint32_t sections,
        uint32_t slices,
        uint32_t* indexOffset,
        uint32_t* vertexOffset,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferIndexView* index);

    void CreateCone(
        float radius,
        float height,
        uint32_t numberOfSections,
        uint32_t* indexOffset,
        uint32_t* vertexOffset,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float3>* normal);

    bool CreateCylinder(
        float radius,
        float height,
        uint32_t sections,
        uint32_t* indexOffset,
        uint32_t* vertexOffset,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferIndexView* index);

    bool CreatePlane(
        const Vector3& p1,
        const Vector3& p2,
        const std::array<Vector2, 4> uvPoints,
        uint32_t* indexOffset,
        uint32_t* vertexOffset,
        AssetBuffer::BufferStructuredView<float2>* uvs,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferIndexView* index);

    bool CreatePlane(
        const Vector3& p1,
        const Vector3& p2,
        uint32_t* indexOffset,
        uint32_t* vertexOffset,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferIndexView* index);

    bool CreateBox(
        const Vector3& boxSize,
        uint32_t* indexOffset,
        uint32_t* vertexOffset,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferStructuredView<float2>* uv0,
        AssetBuffer::BufferIndexView* index);

    void CreateUVSphere(
        float radius,
        uint32_t verticalSlices,
        uint32_t horizontalSlices,
        uint32_t* indexOffset,
        uint32_t* vertexOffset,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferIndexView* index
    );

}
