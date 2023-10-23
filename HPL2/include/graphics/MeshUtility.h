
#pragma once

#include "graphics/AssetBuffer.h"
#include <cstdint>
#include <functional>
#include <variant>

#include "Common_3/Utilities/Math/MathTypes.h"
#include "FixPreprocessor.h"

namespace hpl::MeshUtility {
    struct MeshCreateResult {
        uint32_t numVertices;
        uint32_t numIndices;
    };

    // for create might have to offset the indecies
    //void OffsetIndecies(uint32_t vtxOffset, uint32_t numIndecies, AssetBuffer::BufferIndexView* index);

    void Transform(
        uint32_t numElements,
        const Matrix4& transform,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferStructuredView<float3>* tangent
    );

    void MikkTSpaceGenerate(
        uint32_t numVerts,
        uint32_t numIndecies,
        AssetBuffer::BufferIndexView* view,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float2>* uv,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferStructuredView<float3>* writeTangent
    );

    void CreateTriTangentVectorsHPL2(
        uint32_t numVerts,
        uint32_t numIndecies,
        AssetBuffer::BufferIndexView* index,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float2>* uv,
        AssetBuffer::BufferStructuredView<float3>* normal,
        std::variant<AssetBuffer::BufferStructuredView<float3>*, AssetBuffer::BufferStructuredView<float4>*> tangent
    );

    MeshCreateResult CreateSphere(
        float afRadius,
        uint32_t alSections,
        uint32_t alSlices,
        AssetBuffer::BufferIndexView* index,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float4>* color,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferStructuredView<float2>* uv,
        std::variant<AssetBuffer::BufferStructuredView<float3>*, AssetBuffer::BufferStructuredView<float4>*> tangent
    );

    MeshCreateResult CreateBox(
        float3 size,
        AssetBuffer::BufferIndexView* index,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float4>* color,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferStructuredView<float2>* uv,
        std::variant<AssetBuffer::BufferStructuredView<float3>*, AssetBuffer::BufferStructuredView<float4>*> tangent
    );
    MeshCreateResult CreateCylinder(
        Vector2 size,
        uint32_t sections,
        AssetBuffer::BufferIndexView* index,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float4>* color,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferStructuredView<float2>* uv,
        std::variant<AssetBuffer::BufferStructuredView<float3>*, AssetBuffer::BufferStructuredView<float4>*> tangent);

    MeshCreateResult CreateCapsule(
        Vector2 avSize,
        int alSections,
        int alSlices,
        AssetBuffer::BufferIndexView* index,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float4>* color,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferStructuredView<float2>* uv,
        std::variant<AssetBuffer::BufferStructuredView<float3>*, AssetBuffer::BufferStructuredView<float4>*> tangent

    );

    MeshCreateResult CreatePlane(
        const std::array<Vector3, 2> corners,
        const std::array<Vector2,4> uvCorners,
        AssetBuffer::BufferIndexView* index,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float4>* color,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferStructuredView<float2>* uv,
        std::variant<AssetBuffer::BufferStructuredView<float3>*, AssetBuffer::BufferStructuredView<float4>*> tangent
    );

    MeshCreateResult CreateCone(const float2 avSize, int alSections,
        AssetBuffer::BufferStructuredView<float3>* position,
        AssetBuffer::BufferStructuredView<float4>* color,
        AssetBuffer::BufferStructuredView<float3>* normal,
        AssetBuffer::BufferStructuredView<float2>* uv,
        std::variant<AssetBuffer::BufferStructuredView<float3>*, AssetBuffer::BufferStructuredView<float4>*> tangent);
}


