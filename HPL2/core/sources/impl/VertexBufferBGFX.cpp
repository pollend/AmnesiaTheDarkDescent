/*
 * Copyright © 2009-2020 Frictional Games
 *
 * This file is part of Amnesia: The Dark Descent.
 *
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "impl/VertexBufferBGFX.h"

#include "absl/types/span.h"
#include "bgfx/bgfx.h"
#include "bgfx/defines.h"
#include "graphics/GraphicsContext.h"
#include "math/Math.h"
#include "system/LowLevelSystem.h"

#include "impl/LowLevelGraphicsSDL.h"

#include <GL/glew.h>
#include <SDL2/SDL_stdinc.h>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace hpl
{

    size_t iVertexBufferBGFX::GetSizeFromHPL(eVertexBufferElementFormat format)
    {
        switch (format)
        {
        case eVertexBufferElementFormat_Float:
            return sizeof(float);
        case eVertexBufferElementFormat_Int:
            return sizeof(uint32_t);
        case eVertexBufferElementFormat_Byte:
            return sizeof(char);
        default:
            break;
        }
        BX_ASSERT(false, "Unknown vertex attribute type.");
        return 0;
    }

    bgfx::AttribType::Enum iVertexBufferBGFX::GetAttribTypeFromHPL(eVertexBufferElementFormat format)
    {
        switch (format)
        {
        case eVertexBufferElementFormat_Float:
            return bgfx::AttribType::Float;
        case eVertexBufferElementFormat_Int:
            return bgfx::AttribType::Float;
        case eVertexBufferElementFormat_Byte:
            return bgfx::AttribType::Uint8;
        }
        BX_ASSERT(false, "Unknown vertex attribute type.");
        return bgfx::AttribType::Count;
    }

    bgfx::Attrib::Enum iVertexBufferBGFX::GetAttribFromHPL(eVertexBufferElement format)
    {
        switch (format)
        {
        case eVertexBufferElement_Normal:
            return bgfx::Attrib::Normal;
        case eVertexBufferElement_Position:
            return bgfx::Attrib::Position;
        case eVertexBufferElement_Color0:
            return bgfx::Attrib::Color0;
        case eVertexBufferElement_Color1:
            return bgfx::Attrib::Color1;
        case eVertexBufferElement_Texture1Tangent:
            return bgfx::Attrib::Tangent;
        case eVertexBufferElement_Texture0:
            return bgfx::Attrib::TexCoord0;
        case eVertexBufferElement_Texture1:
            return bgfx::Attrib::TexCoord1;
        case eVertexBufferElement_Texture2:
            return bgfx::Attrib::TexCoord2;
        case eVertexBufferElement_Texture3:
            return bgfx::Attrib::TexCoord3;
        case eVertexBufferElement_Texture4:
            return bgfx::Attrib::TexCoord4;
        case eVertexBufferElement_User0:
            return bgfx::Attrib::TexCoord5;
        case eVertexBufferElement_User1:
            return bgfx::Attrib::TexCoord6;
        case eVertexBufferElement_User2:
            return bgfx::Attrib::TexCoord7;
        case eVertexBufferElement_User3:
        case eVertexBufferElement_LastEnum:
            break;
        }
        BX_ASSERT(false, "Unknown vertex attribute type.");
        return bgfx::Attrib::Count;
    }

    iVertexBufferBGFX::iVertexBufferBGFX(
        iLowLevelGraphics* apLowLevelGraphics,
        eVertexBufferType aType,
        eVertexBufferDrawType aDrawType,
        eVertexBufferUsageType aUsageType,
        int alReserveVtxSize,
        int alReserveIdxSize)
        : iVertexBuffer(apLowLevelGraphics, aType, aDrawType, aUsageType, alReserveVtxSize, alReserveIdxSize)
    {
    }

    iVertexBufferBGFX::~iVertexBufferBGFX()
    {
        for (auto& element : m_vertexElements)
        {
            if (bgfx::isValid(element.m_handle))
            {
                bgfx::destroy(element.m_handle);
            }
            if (bgfx::isValid(element.m_dynamicHandle))
            {
                bgfx::destroy(element.m_dynamicHandle);
            }
            element.m_dynamicHandle = BGFX_INVALID_HANDLE;
            element.m_handle = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(m_dynamicIndexHandle))
        {
            bgfx::destroy(m_dynamicIndexHandle);
        }
        m_dynamicIndexHandle = BGFX_INVALID_HANDLE;
    }

    static void PushVertexElements(
        absl::Span<const float> values, eVertexBufferElement elementType, absl::Span<iVertexBufferBGFX::VertexElement> elements)
    {
        for (auto& element : elements)
        {
            if (element.m_type == elementType)
            {
                auto& buffer = element.m_buffer;
                switch (element.m_format)
                {
                case eVertexBufferElementFormat_Float:
                    {
                        for (size_t i = 0; i < element.m_num; i++)
                        {
                            union {
                                float f;
                                unsigned char b[sizeof(float)];
                            } entry = { i < values.size() ? values[i] : 0 };
                            buffer.insert(buffer.end(), std::begin(entry.b), std::end(entry.b));
                        }
                    }
                    break;
                case eVertexBufferElementFormat_Int:
                    {
                        for (size_t i = 0; i < element.m_num; i++)
                        {
                            union {
                                int f;
                                unsigned char b[sizeof(int)];
                            } entry = { i < values.size() ? static_cast<int>(values[i]) : 0 };
                            buffer.insert(buffer.end(), std::begin(entry.b), std::end(entry.b));
                        }
                    }
                    break;
                case eVertexBufferElementFormat_Byte:
                    {
                        for (size_t i = 0; i < element.m_num; i++)
                        {
                            buffer.emplace_back(i < values.size() ? static_cast<char>(values[i]) : 0);
                        }
                    }
                    break;
                default:
                    break;
                }
                return;
            }
        }
    }

    size_t iVertexBufferBGFX::VertexElement::Stride() const
    {
        return GetSizeFromHPL(m_format) * m_num;
    }

    size_t iVertexBufferBGFX::VertexElement::NumElements() const
    {
        return m_buffer.size() / Stride();
    }

    void iVertexBufferBGFX::AddVertexVec3f(eVertexBufferElement aElement, const cVector3f& avVtx)
    {
        PushVertexElements(
            absl::MakeSpan(std::begin(avVtx.v), std::end(avVtx.v)),
            aElement,
            absl::MakeSpan(m_vertexElements.begin(), m_vertexElements.end()));
    }

    void iVertexBufferBGFX::AddVertexVec4f(eVertexBufferElement aElement, const cVector3f& avVtx, float afW)
    {
        PushVertexElements(
            absl::MakeSpan(std::begin(avVtx.v), std::end(avVtx.v)),
            aElement,
            absl::MakeSpan(m_vertexElements.begin(), m_vertexElements.end()));
    }

    void iVertexBufferBGFX::AddVertexColor(eVertexBufferElement aElement, const cColor& aColor)
    {
        PushVertexElements(
            absl::MakeSpan(std::begin(aColor.v), std::end(aColor.v)),
            aElement,
            absl::MakeSpan(m_vertexElements.begin(), m_vertexElements.end()));
    }

    void iVertexBufferBGFX::AddIndex(unsigned int alIndex)
    {
        m_indices.push_back(alIndex);
    }

    void iVertexBufferBGFX::Transform(const cMatrixf& mtxTransform)
    {
        cMatrixf mtxRot = mtxTransform.GetRotation();
        cMatrixf mtxNormalRot = cMath::MatrixInverse(mtxRot).GetTranspose();

        ///////////////
        // Get position
        auto positionElement = std::find_if(
            m_vertexElements.begin(),
            m_vertexElements.end(),
            [](const auto& element)
            {
                return element.m_type == eVertexBufferElement_Position;
            });
        if (positionElement != m_vertexElements.end())
        {
            BX_ASSERT(positionElement->m_format == eVertexBufferElementFormat_Float, "Only float format supported");
            BX_ASSERT(positionElement->m_num == 3, "Only 3 component format supported");
            struct PackedVec3
            {
                float x;
                float y;
                float z;
            };
            auto positionIt = positionElement->GetElements<PackedVec3>();
            for (auto& position : positionIt)
            {
                cVector3f vPos = cMath::MatrixMul(mtxTransform, cVector3f(position.x, position.y, position.z));
                position = { vPos.x, vPos.y, vPos.z };
            }
        }

        auto normalElement = std::find_if(
            m_vertexElements.begin(),
            m_vertexElements.end(),
            [](const auto& element)
            {
                return element.m_type == eVertexBufferElement_Normal;
            });
        if (normalElement != m_vertexElements.end())
        {
            BX_ASSERT(normalElement->m_format == eVertexBufferElementFormat_Float, "Only float format supported");
            BX_ASSERT(normalElement->m_num == 3, "Only 3 component format supported");
            struct PackedVec3
            {
                float x;
                float y;
                float z;
            };
            auto normalIt = normalElement->GetElements<PackedVec3>();
            for (auto& normal : normalIt)
            {
                cVector3f vNormal = cMath::MatrixMul(mtxNormalRot, cVector3f(normal.x, normal.y, normal.z));
                normal = { vNormal.x, vNormal.y, vNormal.z };
            }
        }

        auto tangentElement = std::find_if(
            m_vertexElements.begin(),
            m_vertexElements.end(),
            [](const auto& element)
            {
                return element.m_type == eVertexBufferElement_Texture1Tangent;
            });
        if (tangentElement != m_vertexElements.end())
        {
            BX_ASSERT(tangentElement->m_format == eVertexBufferElementFormat_Float, "Only float format supported");
            BX_ASSERT(tangentElement->m_num == 4, "Only 4 component format supported");
            struct PackedVec4
            {
                float x;
                float y;
                float z;
                float w;
            };
            auto tangentIt = tangentElement->GetElements<PackedVec4>();
            for (auto& tangent : tangentIt)
            {
                cVector3f vTangent = cMath::MatrixMul(mtxRot, cVector3f(tangent.x, tangent.y, tangent.z));
                vTangent.Normalize();
                tangent = { vTangent.x, vTangent.y, vTangent.z, tangent.w };
            }
        }

        // ////////////////////////////
        // //Update the data
        tVertexElementFlag vtxFlag = eVertexElementFlag_Position;
        if (normalElement != m_vertexElements.end())
        {
            vtxFlag |= eVertexElementFlag_Normal;
        }
        if (tangentElement != m_vertexElements.end())
        {
            vtxFlag |= eVertexElementFlag_Texture1;
        }

        UpdateData(vtxFlag, false);
    }

    int iVertexBufferBGFX::GetElementNum(eVertexBufferElement aElement)
    {
        auto element = std::find_if(
            m_vertexElements.begin(),
            m_vertexElements.end(),
            [aElement](const auto& element)
            {
                return element.m_type == aElement;
            });
        if (element != m_vertexElements.end())
        {
            return element->m_num;
        }
        return 0;
    }
    eVertexBufferElementFormat iVertexBufferBGFX::GetElementFormat(eVertexBufferElement aElement)
    {
        auto element = std::find_if(
            m_vertexElements.begin(),
            m_vertexElements.end(),
            [aElement](const auto& element)
            {
                return element.m_type == aElement;
            });
        if (element != m_vertexElements.end())
        {
            return element->m_format;
        }
        BX_ASSERT(false, "Element not found")
        return eVertexBufferElementFormat_LastEnum;
    }

    int iVertexBufferBGFX::GetElementProgramVarIndex(eVertexBufferElement aElement)
    {
        auto element = std::find_if(
            m_vertexElements.begin(),
            m_vertexElements.end(),
            [aElement](const auto& element)
            {
                return element.m_type == aElement;
            });
        if (element != m_vertexElements.end())
        {
            return element->m_programVarIndex;
        }
        BX_ASSERT(false, "program Var Index not found")
        return 0;
    }

    bool iVertexBufferBGFX::Compile(tVertexCompileFlag aFlags)
    {
        if (aFlags & eVertexCompileFlag_CreateTangents)
        {
            CreateElementArray(eVertexBufferElement_Texture1Tangent, eVertexBufferElementFormat_Float, 4);

            auto positionElement = std::find_if(
                m_vertexElements.begin(),
                m_vertexElements.end(),
                [](const auto& element)
                {
                    return element.m_type == eVertexBufferElement_Position;
                });
            auto normalElement = std::find_if(
                m_vertexElements.begin(),
                m_vertexElements.end(),
                [](const auto& element)
                {
                    return element.m_type == eVertexBufferElement_Normal;
                });
            auto textureElement = std::find_if(
                m_vertexElements.begin(),
                m_vertexElements.end(),
                [](const auto& element)
                {
                    return element.m_type == eVertexBufferElement_Texture0;
                });
            auto tangentElement = std::find_if(
                m_vertexElements.begin(),
                m_vertexElements.end(),
                [](const auto& element)
                {
                    return element.m_type == eVertexBufferElement_Texture1Tangent;
                });

            BX_ASSERT(positionElement != m_vertexElements.end(), "No position element found");
            BX_ASSERT(normalElement != m_vertexElements.end(), "No normal element found");
            BX_ASSERT(textureElement != m_vertexElements.end(), "No texture element found");
            BX_ASSERT(tangentElement != m_vertexElements.end(), "No tangent element found");
            BX_ASSERT(positionElement->m_format == eVertexBufferElementFormat_Float, "Only float format supported");
            BX_ASSERT(normalElement->m_format == eVertexBufferElementFormat_Float, "Only float format supported");
            BX_ASSERT(textureElement->m_format == eVertexBufferElementFormat_Float, "Only float format supported");
            BX_ASSERT(tangentElement->m_format == eVertexBufferElementFormat_Float, "Only float format supported");
            BX_ASSERT(positionElement->m_num == 3, "Only 3 component format supported");
            BX_ASSERT(normalElement->m_num == 3, "Only 3 component format supported");
            BX_ASSERT(textureElement->m_num == 2, "Only 2 component format supported");
            BX_ASSERT(tangentElement->m_num == 4, "Only 4 component format supported");

            cMath::CreateTriTangentVectors(
                (float*)tangentElement->m_buffer.data(),
                m_indices.data(),
                m_indices.size(),
                reinterpret_cast<float*>(positionElement->m_buffer.data()),
                positionElement->m_num,
                reinterpret_cast<float*>(textureElement->m_buffer.data()),
                reinterpret_cast<float*>(normalElement->m_buffer.data()),
                positionElement->NumElements());
        }

        for (auto& element : m_vertexElements)
        {
            bgfx::VertexLayout layout{};
            layout.begin().add(GetAttribFromHPL(element.m_type), element.m_num, GetAttribTypeFromHPL(element.m_format)).end();
            switch (mUsageType)
            {
            case eVertexBufferUsageType_Static:
                element.m_handle = bgfx::createVertexBuffer(bgfx::copy(element.m_buffer.data(), element.m_buffer.size()), layout);
                break;
            case eVertexBufferUsageType_Dynamic:
            case eVertexBufferUsageType_Stream:
                element.m_dynamicHandle =
                    bgfx::createDynamicVertexBuffer(bgfx::copy(element.m_buffer.data(), element.m_buffer.size()), layout);
                break;
            default:
                BX_ASSERT(false, "Unknown usage type %d", mUsageType);
                break;
            }
        }
        m_dynamicIndexHandle = bgfx::createDynamicIndexBuffer(bgfx::copy(m_indices.data(), m_indices.size() * sizeof(uint32_t)), BGFX_BUFFER_INDEX32);
        return true;
    }

    void iVertexBufferBGFX::UpdateData(tVertexElementFlag aTypes, bool abIndices)
    {
        for (auto& element : m_vertexElements)
        {
            if (aTypes & element.m_flag)
            {
                switch (mUsageType)
                {
                case eVertexBufferUsageType_Static: {
                    if(bgfx::isValid(element.m_handle)) {
                        bgfx::destroy(element.m_handle);
                    }
                    bgfx::VertexLayout layout{};
                    layout.begin().add(GetAttribFromHPL(element.m_type), element.m_num, GetAttribTypeFromHPL(element.m_format)).end();
                    element.m_handle = bgfx::createVertexBuffer(bgfx::copy(element.m_buffer.data(), element.m_buffer.size()), layout);
                }
                    break;
                case eVertexBufferUsageType_Dynamic:
                case eVertexBufferUsageType_Stream:
                    bgfx::update(element.m_dynamicHandle, 0, bgfx::copy(element.m_buffer.data(), element.m_buffer.size()));
                    break;
                default:
                    BX_ASSERT(false, "Unknown usage type %d", mUsageType);
                    break;
                }
            }
        }
        if (abIndices)
        {
            bgfx::update(m_dynamicIndexHandle, 0, bgfx::copy(m_indices.data(), m_indices.size() * sizeof(uint32_t)));
        }
    }

    float* iVertexBufferBGFX::GetFloatArray(eVertexBufferElement aElement)
    {
        auto element = std::find_if(
            m_vertexElements.begin(),
            m_vertexElements.end(),
            [aElement](const auto& element)
            {
                return element.m_type == aElement;
            });
        if (element != m_vertexElements.end())
        {
            return reinterpret_cast<float*>(element->m_buffer.data());
        }
        return nullptr;
    }

    int* iVertexBufferBGFX::GetIntArray(eVertexBufferElement aElement)
    {
        auto element = std::find_if(
            m_vertexElements.begin(),
            m_vertexElements.end(),
            [aElement](const auto& element)
            {
                return element.m_type == aElement;
            });
        if (element != m_vertexElements.end())
        {
            return reinterpret_cast<int*>(element->m_buffer.data());
        }
        return nullptr;
    }

    unsigned char* iVertexBufferBGFX::GetByteArray(eVertexBufferElement aElement)
    {
        auto element = std::find_if(
            m_vertexElements.begin(),
            m_vertexElements.end(),
            [aElement](const auto& element)
            {
                return element.m_type == aElement;
            });
        if (element != m_vertexElements.end())
        {
            return element->m_buffer.data();
        }
        return nullptr;
    }

    unsigned int* iVertexBufferBGFX::GetIndices()
    {
        return m_indices.data();
    }

    void iVertexBufferBGFX::ResizeArray(eVertexBufferElement aElement, int alSize)
    {
        auto element = std::find_if(
            m_vertexElements.begin(),
            m_vertexElements.end(),
            [aElement](const auto& element)
            {
                return element.m_type == aElement;
            });
        if (element != m_vertexElements.end())
        {
            element->m_buffer.resize(alSize * element->Stride());
        }
    }

    void iVertexBufferBGFX::ResizeIndices(int alSize)
    {
        m_indices.resize(alSize);
    }

    void iVertexBufferBGFX::CreateElementArray(
        eVertexBufferElement aType, eVertexBufferElementFormat aFormat, int alElementNum, int alProgramVarIndex)
    {
        tVertexElementFlag elementFlag = GetVertexElementFlagFromEnum(aType);
        if (elementFlag & mVertexFlags)
        {
            Error("Vertex element of type %d already present in buffer %d!\n", aType, this);
            return;
        }
        mVertexFlags |= elementFlag;

        VertexElement element;
        element.m_buffer.resize(alElementNum * GetSizeFromHPL(aFormat));
        element.m_type = aType;
        element.m_flag = elementFlag;
        element.m_format = aFormat;
        element.m_num = alElementNum;
        element.m_programVarIndex = alProgramVarIndex;
        m_vertexElements.push_back(std::move(element));
    }

    int iVertexBufferBGFX::GetVertexNum()
    {
        auto positionElement = std::find_if(
            m_vertexElements.begin(),
            m_vertexElements.end(),
            [](const auto& element)
            {
                return element.m_type == eVertexBufferElement_Position;
            });
        BX_ASSERT(positionElement != m_vertexElements.end(), "No position element found");
        return positionElement->NumElements();
    }

    int iVertexBufferBGFX::GetIndexNum()
    {
        return m_indices.size();
    }

    cBoundingVolume iVertexBufferBGFX::CreateBoundingVolume()
    {
        cBoundingVolume bv;
        if ((mVertexFlags & eVertexElementFlag_Position) == 0)
        {
            Warning("Could not create bounding volume from buffer %d  because no position element was present!\n", this);
            return bv;
        }

        auto positionElement = std::find_if(
            m_vertexElements.begin(),
            m_vertexElements.end(),
            [](const auto& element)
            {
                return element.m_type == eVertexBufferElement_Position;
            });

        if (positionElement == m_vertexElements.end())
        {
            return bv;
        }

        if (positionElement->m_format != eVertexBufferElementFormat_Float)
        {
            Warning("Could not breate bounding volume since postion was not for format float in buffer %d!\n", this);
            return bv;
        }

        bv.AddArrayPoints(reinterpret_cast<float*>(positionElement->m_buffer.data()), GetVertexNum());
        bv.CreateFromPoints(positionElement->m_num);

        return bv;
    }

    void iVertexBufferBGFX::Draw(eVertexBufferDrawType aDrawType)
    {
    }
    void iVertexBufferBGFX::DrawIndices(unsigned int* apIndices, int alCount, eVertexBufferDrawType aDrawType)
    {
        BX_ASSERT(false, "Not implemented");
    }

    // void iVertexBufferBGFX::Submit(GraphicsContext& context, eVertexBufferDrawType aDrawType) {

    //     GraphicsContext::LayoutStream layoutStream;
    //     layoutStream.m_drawType = aDrawType == eVertexBufferDrawType_LastEnum ?  mDrawType : aDrawType;
    //     for(auto& element: m_vertexElements) {
    //         layoutStream.m_vertexStreams.push_back({
    //             .m_handle = element.m_handle,
    //             .m_dynamicHandle = element.m_dynamicHandle,
    //         });
    //     }
    //     layoutStream.m_indexStream = {
    //         .m_handle = BGFX_INVALID_HANDLE,
    //         .m_dynamicHandle = m_dynamicIndexHandle,
    //     };
    //     context.SubmitLayoutStream(layoutStream);
    // }

    void iVertexBufferBGFX::GetLayoutStream(GraphicsContext::LayoutStream& layoutStream, 
        eVertexBufferDrawType aDrawType) {
        layoutStream.m_drawType = aDrawType == eVertexBufferDrawType_LastEnum ?  mDrawType : aDrawType;
        for(auto& element: m_vertexElements) {
            layoutStream.m_vertexStreams.push_back({
                .m_handle = element.m_handle,
                .m_dynamicHandle = element.m_dynamicHandle,
            });
        }
        layoutStream.m_indexStream = {
            .m_handle = BGFX_INVALID_HANDLE,
            .m_dynamicHandle = m_dynamicIndexHandle,
        };
    }



    void iVertexBufferBGFX::Bind()
    {
        uint8_t stream = 0;
        for (auto& element : m_vertexElements)
        {
            if (bgfx::isValid(element.m_handle))
            {
                bgfx::setVertexBuffer(++stream, element.m_handle);
            }
            else
            {
                bgfx::setVertexBuffer(++stream, element.m_dynamicHandle);
            }
        }
        bgfx::setIndexBuffer(m_dynamicIndexHandle);
    }
    void iVertexBufferBGFX::UnBind()
    {
        return;
    }

    iVertexBuffer* iVertexBufferBGFX::CreateCopy(eVertexBufferType aType, eVertexBufferUsageType aUsageType, tVertexElementFlag alVtxToCopy)
    {
        auto* vertexBuffer =
            new iVertexBufferBGFX(mpLowLevelGraphics, eVertexBufferType_Hardware, mDrawType, aUsageType, GetIndexNum(), GetVertexNum());
        vertexBuffer->m_indices = m_indices;

        for (auto element : m_vertexElements)
        {
            if (element.m_flag & alVtxToCopy)
            {
                auto& eleCopy = vertexBuffer->m_vertexElements.emplace_back(element);
                eleCopy.m_handle = BGFX_INVALID_HANDLE;
                eleCopy.m_dynamicHandle = BGFX_INVALID_HANDLE;
            }
        }
        vertexBuffer->m_dynamicIndexHandle = BGFX_INVALID_HANDLE;
        vertexBuffer->Compile(0);

        return vertexBuffer;
    }

} // namespace hpl
