#include "absl/types/span.h"
#include "bgfx/bgfx.h"
#include "graphics/BufferIndex.h"
#include "math/MathTypes.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <graphics/BufferVertex.h>
#include <math/BoundingVolume.h>
#include <math/Math.h>
#include <utility>
#include <vector>

namespace hpl
{
    BufferVertex::BufferVertex(const VertexDefinition& defintion, std::vector<char>&& data)
        : _definition(defintion)
        , _buffer(std::move(data))
    {
    }
    BufferVertex::BufferVertex(const VertexDefinition& defintion)
        : _definition(defintion)
    {
    }

    BufferVertex::BufferVertex(const VertexDefinition& defintion, std::vector<char>& data)
        : _definition(defintion)
        , _buffer(data)
    {
    }

    BufferVertex::BufferVertex(BufferVertex&& vtxData)
        : _definition(vtxData._definition)
        , _buffer(std::move(vtxData._buffer))
        , _handle(vtxData._handle)
    {
        vtxData._handle = BGFX_INVALID_HANDLE;
        vtxData._dynamicHandle = BGFX_INVALID_HANDLE;
    }

    BufferVertex::~BufferVertex()
    {
        if (bgfx::isValid(_handle))
        {
            bgfx::destroy(_handle);
        }
        if (bgfx::isValid(_dynamicHandle))
        {
            bgfx::destroy(_dynamicHandle);
        }
    }

    void BufferVertex::operator=(BufferVertex&& other)
    {
        _definition = other._definition;
        _buffer = std::move(other._buffer);
        _handle = other._handle;

        other._handle = BGFX_INVALID_HANDLE;
        other._dynamicHandle = BGFX_INVALID_HANDLE;
    }

    void BufferVertexView::Transform(const cMatrixf& mtx)
    {
        const cMatrixf mtxRot = mtx.GetRotation();
        const cMatrixf mtxNormalRot = cMath::MatrixInverse(mtxRot).GetTranspose();

        auto& definition = GetVertexBuffer().Definition();
        const bool hasPosition = definition.m_layout.has(bgfx::Attrib::Position);
        const bool hasTangents = definition.m_layout.has(bgfx::Attrib::Tangent);
        const bool hasNormal = definition.m_layout.has(bgfx::Attrib::Normal);

        auto rawBuffer = GetBuffer();
        // const size_t numIndicies = _vertexBuffer->size() / _definition.m_layout.getStride();
        for (size_t i = 0; i < NumberElements(); i++)
        {
            if (hasPosition)
            {
                float input[4] = { 0, 0, 0, 0 };
                bgfx::vertexUnpack(input, bgfx::Attrib::Position, definition.m_layout, rawBuffer.data(), i);
                cVector3f vPos = cMath::MatrixMul(mtx, cVector3f(input[0], input[1], input[2]));
                const float output[4] = { vPos.x, vPos.y, vPos.z, 0 };
                bgfx::vertexPack(output, false, bgfx::Attrib::Position, definition.m_layout, rawBuffer.data());
            }

            if (hasTangents)
            {
                float input[4] = { 0, 0, 0, 0 };
                bgfx::vertexUnpack(input, bgfx::Attrib::Tangent, definition.m_layout, rawBuffer.data(), i);
                cVector3f vPos = cMath::MatrixMul(mtxRot, cVector3f(input[0], input[1], input[2]));
                const float output[4] = { vPos.x, vPos.y, vPos.z, 0 };
                bgfx::vertexPack(output, false, bgfx::Attrib::Tangent, definition.m_layout, rawBuffer.data());
            }
            if (hasNormal)
            {
                float input[4] = { 0, 0, 0, 0 };
                bgfx::vertexUnpack(input, bgfx::Attrib::Normal, definition.m_layout, rawBuffer.data(), i);
                cVector3f vPos = cMath::MatrixMul(mtxNormalRot, cVector3f(input[0], input[1], input[2]));
                const float output[4] = { vPos.x, vPos.y, vPos.z, 0 };
                bgfx::vertexPack(output, false, bgfx::Attrib::Normal, definition.m_layout, rawBuffer.data());
            }
        }
    }

    const BufferVertex::VertexDefinition& BufferVertex::Definition() const
    {
        return _definition;
    }

    absl::Span<char> BufferVertex::GetBuffer()
    {
        return absl::Span<char>(_buffer.data(), _buffer.size());
    }

    absl::Span<const char> BufferVertex::GetConstBuffer()
    {
        return absl::Span<const char>(_buffer.data(), _buffer.size());
    }

    void BufferVertex::Update()
    {
        Update(0, NumberElements());
    }

    void BufferVertex::Update(size_t offset, size_t size)
    {
        switch(_definition.m_accessType) {
        case AccessType::AccessStatic: {
            Invalidate();
            Initialize();
            break;
        }
            break;
        case AccessType::AccessDynamic:
        case AccessType::AccessStream:
            BX_ASSERT(bgfx::isValid(_dynamicHandle), "Dynamic handle is invalid");
            BX_ASSERT(offset + size <= NumberElements(), "Offset + size is greater than number of elements");
            bgfx::update(_dynamicHandle, offset, bgfx::makeRef(_buffer.data() + offset * Stride(), size * Stride()));
            break;
        }
    }

    void BufferVertex::Resize(size_t count)
    {
        _buffer.resize(count * _definition.m_layout.getStride());
    }

    void BufferVertex::Reserve(size_t count)
    {
        _buffer.reserve(count * _definition.m_layout.getStride());
    }

    size_t BufferVertex::NumberBytes() const
    {
        return _buffer.size();
    }

    size_t BufferVertex::Stride() const
    {
        return _definition.m_layout.getStride();
    }

    size_t BufferVertex::NumberElements() const
    {
        return _buffer.size() / _definition.m_layout.getStride();
    }

    cBoundingVolume BufferVertexView::GetBoundedVolume()
    {
        auto& definition = _vertexBuffer->GetDefinition();
        auto rawBuffer = GetBuffer();

        if (definition.m_layout.has(bgfx::Attrib::Position))
        {
            const size_t numIndicies = NumberElements();
            cVector3f min(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
            cVector3f max(std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min());
            for (size_t i = 0; i < numIndicies; i++)
            {
                float output[4] = { 0, 0, 0, 0 };
                bgfx::vertexUnpack(output, bgfx::Attrib::Position, definition.m_layout, rawBuffer.data(), i);

                min.x = std::min(min.x, output[0]);
                min.y = std::min(min.y, output[1]);
                min.z = std::min(min.z, output[2]);

                max.x = std::min(max.x, output[0]);
                max.y = std::min(max.y, output[1]);
                max.z = std::min(max.z, output[2]);
            }
            cBoundingVolume volume;
            volume.SetLocalMinMax(min, max);
            return volume;
        }

        return cBoundingVolume();
    }

    void BufferVertexView::UpdateTangentsFromPositionTexCoords(const BufferIndexView& index)
    {
        auto& definition = _vertexBuffer->GetDefinition();

        BX_ASSERT(definition.m_layout.has(bgfx::Attrib::Position), "must have bgfx::Attrib::Position attribute");
        BX_ASSERT(definition.m_layout.has(bgfx::Attrib::TexCoord0), "must have bgfx::Attrib::TexCoord0 attribute");
        BX_ASSERT(definition.m_layout.has(bgfx::Attrib::Normal), "must have bgfx::Attrib::Normal attribute");
        BX_ASSERT(definition.m_layout.has(bgfx::Attrib::Tangent), "must have bgfx::Attrib::Tangent attribute");

        std::vector<cVector3f> sTangents;
        std::vector<cVector3f> rTangents;
        sTangents.resize(NumberElements());
        rTangents.resize(NumberElements());
        auto rawBuffer = GetBuffer();

        auto updateTangents = [&](auto indexBuffer) -> void
        {
            for (auto i = 0; i < indexBuffer.size(); i += 3)
            {
                std::array<cVector3f, 3> triPos;
                std::array<cVector3f, 3> texCoordPos;
                for (size_t i = 0; i < 3; ++i)
                {
                    const auto idx = indexBuffer[i + 0];

                    float pos[4] = { 0 };
                    float texCoord[4] = { 0 };
                    bgfx::vertexUnpack(pos, bgfx::Attrib::Position, definition.m_layout, rawBuffer.data(), idx);
                    bgfx::vertexUnpack(texCoord, bgfx::Attrib::TexCoord0, definition.m_layout, rawBuffer.data(), idx);

                    triPos[i] = cVector3f(pos[0], pos[1], pos[2]);
                    texCoordPos[i] = cVector3f(texCoord[0], texCoord[1], texCoord[2]);
                }

                // Get the vectors between the positions.
                cVector3f vPos1To2 = triPos[1] - triPos[0];
                cVector3f vPos1To3 = triPos[2] - triPos[0];

                // Get the vectors between the tex coords
                cVector3f vTex1To2 = texCoordPos[1] - texCoordPos[0];
                cVector3f vTex1To3 = texCoordPos[2] - texCoordPos[0];

                // Get the direction of the S and T tangents
                float fR = 1.0f / (vTex1To2.x * vTex1To3.y - vTex1To2.y * vTex1To3.x);

                cVector3f vSDir(
                    (vTex1To3.y * vPos1To2.x - vTex1To2.y * vPos1To3.x) * fR,
                    (vTex1To3.y * vPos1To2.y - vTex1To2.y * vPos1To3.y) * fR,
                    (vTex1To3.y * vPos1To2.z - vTex1To2.y * vPos1To3.z) * fR);

                cVector3f vTDir(
                    (vTex1To2.x * vPos1To3.x - vTex1To3.x * vPos1To2.x) * fR,
                    (vTex1To2.x * vPos1To3.y - vTex1To3.x * vPos1To2.y) * fR,
                    (vTex1To2.x * vPos1To3.z - vTex1To3.x * vPos1To2.z) * fR);

                // Add the temp arrays with the values:
                sTangents[i + 0] = vSDir;
                sTangents[i + 1] = vSDir;
                sTangents[i + 2] = vSDir;

                rTangents[i + 0] = vTDir;
                rTangents[i + 1] = vTDir;
                rTangents[i + 2] = vTDir;
            }

            // Go through the vertrices and find normal and vertex copies. Smooth the tangents on these
            const float fMaxCosAngle = -1.0f;
            for (size_t i = 0; i < NumberElements(); ++i)
            {
                for (size_t j = 1 + i; j < NumberElements(); ++j)
                {
                    float iPos[4] = { 0 };
                    float iNormal[4] = { 0 };
                    bgfx::vertexUnpack(iPos, bgfx::Attrib::Position, definition.m_layout, rawBuffer.data(), i);
                    bgfx::vertexUnpack(iNormal, bgfx::Attrib::Normal, definition.m_layout, rawBuffer.data(), i);

                    float jPos[4] = { 0 };
                    float jNormal[4] = { 0 };
                    bgfx::vertexUnpack(jPos, bgfx::Attrib::Position, definition.m_layout, rawBuffer.data(), j);
                    bgfx::vertexUnpack(jNormal, bgfx::Attrib::Normal, definition.m_layout, rawBuffer.data(), j);

                    if (cMath::Vector3EqualEpsilon(cVector3f(iPos[0], iPos[1], iPos[2]), cVector3f(jPos[0], jPos[1], jPos[0])) &&
                        cMath::Vector3EqualEpsilon(
                            cVector3f(jNormal[0], jNormal[1], jNormal[2]), cVector3f(jNormal[0], jNormal[1], jNormal[0])))
                    {
                        cVector3f vAT1 = sTangents[i];
                        cVector3f vAT2 = rTangents[i];

                        cVector3f vBT1 = sTangents[j];
                        cVector3f vBT2 = rTangents[j];

                        if (cMath::Vector3Dot(vAT1, vBT1) >= fMaxCosAngle)
                        {
                            sTangents[j] += vAT1;
                            sTangents[i] += vBT1;
                        }

                        if (cMath::Vector3Dot(vAT2, vBT2) >= fMaxCosAngle)
                        {
                            rTangents[j] += vAT2;
                            rTangents[i] += vBT2;
                        }
                    }
                }
            }

            for (size_t i = 0; i < NumberElements(); ++i)
            {
                cVector3f& sTangent = sTangents[i];
                cVector3f& rTangent = rTangents[i];
                float normal[4] = { 0 };
                bgfx::vertexUnpack(normal, bgfx::Attrib::Normal, definition.m_layout, rawBuffer.data(), i);
                cVector3f normalVec = cVector3f(normal[0], normal[1], normal[2]);

                // Gram-Schmidt orthogonalize
                cVector3f vTan = sTangent - (normalVec * cMath::Vector3Dot(normalVec, sTangent));
                vTan.Normalize();

                float fW = (cMath::Vector3Dot(cMath::Vector3Cross(normalVec, sTangent), rTangent) < 0.0f) ? -1.0f : 1.0f;

                float tangent[4] = { vTan.x, vTan.y, vTan.z, fW };
                bgfx::vertexPack(tangent, false, bgfx::Attrib::Tangent, definition.m_layout, rawBuffer.data(), i);
            }
        };
        switch (index.GetIndexBuffer().GetDefinition().m_format)
        {
        case BufferIndex::IndexFormatType::Uint16:
            updateTangents(index.GetConstBufferUint16());
            break;
        case BufferIndex::IndexFormatType::Uint32:
            updateTangents(index.GetConstBufferUint32());
            break;
        }
    }

    void BufferVertex::Initialize()
    {
        switch (_definition.m_accessType)
        {
        case BufferVertex::AccessType::AccessStatic:
            if (!bgfx::isValid(_handle))
            {
                _handle = bgfx::createVertexBuffer(bgfx::makeRef(_buffer.data(), _buffer.size()), _definition.m_layout);
            }
            break;
        case BufferVertex::AccessType::AccessDynamic:
        case BufferVertex::AccessType::AccessStream:
            if (!bgfx::isValid(_dynamicHandle))
            {
                _dynamicHandle = bgfx::createDynamicVertexBuffer(bgfx::makeRef(_buffer.data(), _buffer.size()), _definition.m_layout);
            }
            break;
        }
    }

    void BufferVertexView::Update() {
        BX_ASSERT(_vertexBuffer, "View is Invalid");
        _vertexBuffer->Update(_offset, _count);
    }

    absl::Span<char> BufferVertexView::GetBuffer()
    {
        if(IsEmpty()) {
            return {};
        }
        return _vertexBuffer->GetBuffer().subspan(_offset * _vertexBuffer->Stride(), _count * _vertexBuffer->Stride());
    }

    absl::Span<const char> BufferVertexView::GetConstBuffer()
    {
        if(IsEmpty()) {
            return {};
        }
        return _vertexBuffer->GetBuffer().subspan(_offset * _vertexBuffer->Stride(), _count * _vertexBuffer->Stride());
    }

    void BufferVertex::Invalidate()
    {
        if (bgfx::isValid(_handle))
        {
            bgfx::destroy(_handle);
            _handle.idx = bgfx::kInvalidHandle;
        }

        if (bgfx::isValid(_dynamicHandle))
        {
            bgfx::destroy(_dynamicHandle);
            _dynamicHandle.idx = bgfx::kInvalidHandle;
        }
    }
} // namespace hpl