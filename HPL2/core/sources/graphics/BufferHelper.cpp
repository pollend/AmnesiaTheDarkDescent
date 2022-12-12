#include "absl/types/span.h"
#include "bgfx/bgfx.h"
#include <graphics/BufferHelper.h>
#include "math/Math.h"
#include "math/MathTypes.h"

namespace hpl::vertex::helper
{
    void SetAttribute(absl::Span<Buffer*> view, size_t index, bgfx::Attrib::Enum attribute, bool normalized, const float input[4])
    {
        for (auto it = view.begin(); it != view.end(); ++it)
        {
            (*it)->SetAttribute(input, index, normalized, attribute);
        }
    }

    bool GetAttribute(absl::Span<Buffer*> view, size_t index, bgfx::Attrib::Enum attribute, float output[4])
    {
        for (auto it = view.begin(); it != view.end(); ++it)
        {
            if((*it)->HasAttribute(attribute)) {
                (*it)->GetAttribute(output, index, attribute);
                return true;
            }
        }
        return false;
    }
    
    bool HasAttribute(absl::Span<Buffer*> view, bgfx::Attrib::Enum attribute) {
        for (auto it = view.begin(); it != view.end(); ++it)
        {
            if((*it)->HasAttribute(attribute)) {
                return true;
            }
        }
        return false;
    }

    void Transform(absl::Span<Buffer*> view, const cMatrixf& mtx, size_t offset, size_t size) {
        const cMatrixf mtxRot = mtx.GetRotation();
        const cMatrixf mtxNormalRot = cMath::MatrixInverse(mtxRot).GetTranspose();


        // const size_t numIndicies = _vertexBuffer->size() / _definition.m_layout.getStride();
        for (size_t i = offset; i < size; i++)
        {
            if (HasAttribute(view, bgfx::Attrib::Position))
            {
                float input[4] = { 0, 0, 0, 0 };
                GetAttribute(view, i, bgfx::Attrib::Position, input);
                cVector3f vPos = cMath::MatrixMul(mtx, cVector3f(input[0], input[1], input[2]));
                const float output[4] = { vPos.x, vPos.y, vPos.z, 0 };
                SetAttribute(view, i, bgfx::Attrib::Position, false, output);
            }

            if (HasAttribute(view, bgfx::Attrib::Tangent))
            {
                float input[4] = { 0, 0, 0, 0 };
                GetAttribute(view, i, bgfx::Attrib::Tangent, input);
                cVector3f vPos = cMath::MatrixMul(mtxRot, cVector3f(input[0], input[1], input[2]));
                const float output[4] = { vPos.x, vPos.y, vPos.z, 0 };
                SetAttribute(view, i, bgfx::Attrib::Tangent, false, output);
            }
            if (HasAttribute(view, bgfx::Attrib::Normal))
            {
                float input[4] = { 0, 0, 0, 0 };
                GetAttribute(view, i, bgfx::Attrib::Normal, input);
                cVector3f vPos = cMath::MatrixMul(mtxNormalRot, cVector3f(input[0], input[1], input[2]));
                const float output[4] = { vPos.x, vPos.y, vPos.z, 0 };
                SetAttribute(view, i, bgfx::Attrib::Normal, false, output);
            }
        }
    }

    cBoundingVolume GetBoundedVolume(absl::Span<Buffer*> view, size_t offset, size_t size) {
        if(!HasAttribute(view, bgfx::Attrib::Position)) {
            return cBoundingVolume();
        }

        cVector3f min(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
        cVector3f max(std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min());
        for(size_t i = offset; i < size; i++) {
            float position[4] = { 0, 0, 0, 0 };
            if(GetAttribute(view, i, bgfx::Attrib::Position, position)) {
                min.x = std::min(min.x, position[0]);
                min.y = std::min(min.y, position[1]);
                min.z = std::min(min.z, position[2]);

                max.x = std::min(max.x, position[0]);
                max.y = std::min(max.y, position[1]);
                max.z = std::min(max.z, position[2]);
            }
        }
        cBoundingVolume volume;
        volume.SetLocalMinMax(min, max);
        return volume;
    }

    void UpdateTangentsFromPositionTexCoords(const Buffer* indexBuffer, absl::Span<Buffer*> view, size_t size) {
          //     auto& definition = Definition();
        if(!indexBuffer) {
            return;
        }

        std::vector<cVector3f> sTangents;
        std::vector<cVector3f> rTangents;
        sTangents.resize(size);
        rTangents.resize(size);
        // auto rawBuffer = GetBuffer();

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
                    GetAttribute(view, idx, bgfx::Attrib::Position, pos);
                    GetAttribute(view, idx, bgfx::Attrib::TexCoord0, texCoord);
                    
                    // bgfx::vertexUnpack(pos, bgfx::Attrib::Position, definition.m_layout, rawBuffer.data(), idx);
                    // bgfx::vertexUnpack(texCoord, bgfx::Attrib::TexCoord0, definition.m_layout, rawBuffer.data(), idx);

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
            for (size_t i = 0; i < size; ++i)
            {
                for (size_t j = 1 + i; j < size; ++j)
                {
                    float iPos[4] = { 0 };
                    float iNormal[4] = { 0 };
                    // bgfx::vertexUnpack(iPos, bgfx::Attrib::Position, definition.m_layout, rawBuffer.data(), i);
                    // bgfx::vertexUnpack(iNormal, bgfx::Attrib::Normal, definition.m_layout, rawBuffer.data(), i);

                    GetAttribute(view, i, bgfx::Attrib::Position, iPos);
                    GetAttribute(view, i, bgfx::Attrib::TexCoord0, iNormal);

                    float jPos[4] = { 0 };
                    float jNormal[4] = { 0 };

                    GetAttribute(view, j, bgfx::Attrib::Position, jPos);
                    GetAttribute(view, j, bgfx::Attrib::TexCoord0, jNormal);

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

            for (size_t i = 0; i < size; ++i)
            {
                cVector3f& sTangent = sTangents[i];
                cVector3f& rTangent = rTangents[i];
                float normal[4] = { 0 };
                GetAttribute(view, i, bgfx::Attrib::Normal, normal);
                // bgfx::vertexUnpack(normal, bgfx::Attrib::Normal, definition.m_layout, rawBuffer.data(), i);
                cVector3f normalVec = cVector3f(normal[0], normal[1], normal[2]);

                // Gram-Schmidt orthogonalize
                cVector3f vTan = sTangent - (normalVec * cMath::Vector3Dot(normalVec, sTangent));
                vTan.Normalize();

                float fW = (cMath::Vector3Dot(cMath::Vector3Cross(normalVec, sTangent), rTangent) < 0.0f) ? -1.0f : 1.0f;

                float tangent[4] = { vTan.x, vTan.y, vTan.z, fW };
                SetAttribute(view, i, bgfx::Attrib::Tangent, false, tangent);
            }
        };
        switch (indexBuffer->GetDefinition().m_format)
        {
        case BufferDefinition::FormatType::Uint16:
            updateTangents(indexBuffer->GetConstElements<uint16_t>());
            break;
        case BufferDefinition::FormatType::Uint32:
            updateTangents(indexBuffer->GetConstElements<uint32_t>());
            break;
        default:
            break;
        }
    }

}; // namespace hpl::vertex::helper