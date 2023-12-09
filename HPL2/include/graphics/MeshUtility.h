
#pragma once

#include "graphics/GraphicsBuffer.h"
#include <cstdint>
#include <functional>
#include <variant>

#include "Common_3/Utilities/Math/MathTypes.h"
#include "FixPreprocessor.h"



namespace hpl::MeshUtility {
    using snorm4_t = uint32_t;
    using unorm4_t = uint32_t;

    struct MeshCreateResult {
        uint32_t numVertices;
        uint32_t numIndices;
    };

    // for create might have to offset the indecies
    //void OffsetIndecies(uint32_t vtxOffset, uint32_t numIndecies, AssetBuffer::BufferIndexView* index);

    void Transform(
        uint32_t numElements,
        const Matrix4& transform,
        GraphicsBuffer::BufferStructuredView<float3>* position,
        GraphicsBuffer::BufferStructuredView<float3>* normal,
        GraphicsBuffer::BufferStructuredView<float3>* tangent
    );

    void MikkTSpaceGenerate(
        uint32_t numVerts,
        uint32_t numIndecies,
        GraphicsBuffer::BufferIndexView* view,
        GraphicsBuffer::BufferStructuredView<float3>* position,
        GraphicsBuffer::BufferStructuredView<float2>* uv,
        std::variant<
            GraphicsBuffer::BufferStructuredView<snorm4_t>*,
            GraphicsBuffer::BufferStructuredView<float3>*> normal,
        GraphicsBuffer::BufferStructuredView<float3>* writeTangent
    );

    void CreateTriTangentVectorsHPL2(
        uint32_t numVerts,
        uint32_t numIndecies,
        GraphicsBuffer::BufferIndexView* index,
        GraphicsBuffer::BufferStructuredView<float3>* position,
        GraphicsBuffer::BufferStructuredView<float2>* uv,
        std::variant<
            GraphicsBuffer::BufferStructuredView<snorm4_t>*,
            GraphicsBuffer::BufferStructuredView<float3>*> normal,
        std::variant<
            GraphicsBuffer::BufferStructuredView<snorm4_t>*,
            GraphicsBuffer::BufferStructuredView<float3>*> tangent
    );

    MeshCreateResult CreateSphere(
        float afRadius,
        uint32_t alSections,
        uint32_t alSlices,
        GraphicsBuffer::BufferIndexView* index,
        GraphicsBuffer::BufferStructuredView<float3>* position,
        std::variant<
            GraphicsBuffer::BufferStructuredView<unorm4_t>*,
            GraphicsBuffer::BufferStructuredView<float4>*> color,
        std::variant<
            GraphicsBuffer::BufferStructuredView<snorm4_t>*,
            GraphicsBuffer::BufferStructuredView<float3>*> normal,
        GraphicsBuffer::BufferStructuredView<float2>* uv,
        std::variant<
            GraphicsBuffer::BufferStructuredView<snorm4_t>*,
            GraphicsBuffer::BufferStructuredView<float3>*> tangent
    );

    MeshCreateResult CreateBox(
        float3 size,
        GraphicsBuffer::BufferIndexView* index,
        GraphicsBuffer::BufferStructuredView<float3>* position,
        std::variant<
            GraphicsBuffer::BufferStructuredView<uint32_t>*,
            GraphicsBuffer::BufferStructuredView<float4>*> color,
        std::variant<
            GraphicsBuffer::BufferStructuredView<snorm4_t>*,
            GraphicsBuffer::BufferStructuredView<float3>*> normal,
        GraphicsBuffer::BufferStructuredView<float2>* uv,
        std::variant<
            GraphicsBuffer::BufferStructuredView<snorm4_t>*,
            GraphicsBuffer::BufferStructuredView<float3>*> tangent);
    MeshCreateResult CreateCylinder(
        Vector2 size,
        uint32_t sections,
        GraphicsBuffer::BufferIndexView* index,
        GraphicsBuffer::BufferStructuredView<float3>* position,
        std::variant<
            GraphicsBuffer::BufferStructuredView<uint32_t>*,
            GraphicsBuffer::BufferStructuredView<float4>*> color,
        std::variant<
            GraphicsBuffer::BufferStructuredView<snorm4_t>*,
            GraphicsBuffer::BufferStructuredView<float3>*> normal,
        GraphicsBuffer::BufferStructuredView<float2>* uv,
        std::variant<
            GraphicsBuffer::BufferStructuredView<snorm4_t>*,
            GraphicsBuffer::BufferStructuredView<float3>*> tangent);

    MeshCreateResult CreateCapsule(
        Vector2 avSize,
        int alSections,
        int alSlices,
        GraphicsBuffer::BufferIndexView* index,
        GraphicsBuffer::BufferStructuredView<float3>* position,
        std::variant<
            GraphicsBuffer::BufferStructuredView<unorm4_t>*,
            GraphicsBuffer::BufferStructuredView<float4>*> color,
        std::variant<
            GraphicsBuffer::BufferStructuredView<snorm4_t>*,
            GraphicsBuffer::BufferStructuredView<float3>*> normal,
        GraphicsBuffer::BufferStructuredView<float2>* uv,
        std::variant<
            GraphicsBuffer::BufferStructuredView<snorm4_t>*,
            GraphicsBuffer::BufferStructuredView<float3>*> tangent);

    MeshCreateResult CreatePlane(
        Vector3 p1,
        Vector3 p2,
        Vector2 uv1,
        Vector2 uv2,
        Vector2 uv3,
        Vector2 uv4,
        GraphicsBuffer::BufferIndexView* index,
        GraphicsBuffer::BufferStructuredView<float3>* position,
        std::variant<
            GraphicsBuffer::BufferStructuredView<uint32_t>*,
            GraphicsBuffer::BufferStructuredView<float4>*> color,
        std::variant<
            GraphicsBuffer::BufferStructuredView<snorm4_t>*,
            GraphicsBuffer::BufferStructuredView<float3>*> normal,
        GraphicsBuffer::BufferStructuredView<float2>* uv,
        std::variant<
            GraphicsBuffer::BufferStructuredView<snorm4_t>*,
            GraphicsBuffer::BufferStructuredView<float3>*> tangent);

    MeshCreateResult CreateCone(const float2 avSize, int alSections,
        GraphicsBuffer::BufferStructuredView<float3>* position,
        std::variant<
            GraphicsBuffer::BufferStructuredView<unorm4_t>*,
            GraphicsBuffer::BufferStructuredView<float4>*> color,
        std::variant<
            GraphicsBuffer::BufferStructuredView<snorm4_t>*,
            GraphicsBuffer::BufferStructuredView<float3>*> normal,
        GraphicsBuffer::BufferStructuredView<float2>* uv,
        std::variant<
            GraphicsBuffer::BufferStructuredView<snorm4_t>*,
            GraphicsBuffer::BufferStructuredView<float3>*> tangent);
}


