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
#pragma once

#include "absl/container/inlined_vector.h"
#include "graphics/VertexBuffer.h"
#include <array>
#include <bgfx/bgfx.h>
#include <bx/debug.h>
#include <cstdint>
#include <vector>

namespace hpl
{

    class iVertexBufferBGFX : public iVertexBuffer
    {
    public:
        static size_t GetSizeFromHPL(eVertexBufferElementFormat format);
        static bgfx::Attrib::Enum GetAttribFromHPL(eVertexBufferElement type);
        static bgfx::AttribType::Enum GetAttribTypeFromHPL(eVertexBufferElementFormat format);

        struct VertexElement
        {
            eVertexBufferElementFormat m_format = eVertexBufferElementFormat::eVertexBufferElementFormat_Float;
            eVertexBufferElement m_type = eVertexBufferElement::eVertexBufferElement_Position;
            tVertexElementFlag m_flag = 0;
            bgfx::VertexBufferHandle m_handle = BGFX_INVALID_HANDLE;
            bgfx::DynamicVertexBufferHandle m_dynamicHandle = BGFX_INVALID_HANDLE;
            size_t m_num = 0;
            int m_programVarIndex = 0;
            std::vector<uint8_t> m_buffer = {};

            size_t Stride() const
            {
                return GetSizeFromHPL(m_format) * m_num;
            }

            size_t NumElements() const
            {
                return m_buffer.size() / Stride();
            }

            template<typename TData>
            absl::Span<TData> GetElements()
            {
                BX_ASSERT(sizeof(TData) == Stride(), "Data must be same size as stride");
                return absl::MakeSpan(reinterpret_cast<TData*>(m_buffer.data()), m_buffer.size() / Stride());
            }
        };

        iVertexBufferBGFX(
            iLowLevelGraphics* apLowLevelGraphics,
            eVertexBufferType aType,
            eVertexBufferDrawType aDrawType,
            eVertexBufferUsageType aUsageType,
            int alReserveVtxSize,
            int alReserveIdxSize);
        ~iVertexBufferBGFX();

        virtual void CreateElementArray(
            eVertexBufferElement aType, eVertexBufferElementFormat aFormat, int alElementNum, int alProgramVarIndex = 0) override;

        virtual void AddVertexVec3f(eVertexBufferElement aElement, const cVector3f& avVtx) override;
        virtual void AddVertexVec4f(eVertexBufferElement aElement, const cVector3f& avVtx, float afW) override;
        virtual void AddVertexColor(eVertexBufferElement aElement, const cColor& aColor) override;
        virtual void AddIndex(unsigned int alIndex) override;

        virtual bool Compile(tVertexCompileFlag aFlags) override;
        virtual void UpdateData(tVertexElementFlag aTypes, bool abIndices) override;

        virtual void Transform(const cMatrixf& mtxTransform) override;

        virtual void Draw(eVertexBufferDrawType aDrawType = eVertexBufferDrawType_LastEnum) override;
        virtual void DrawIndices(	unsigned int *apIndices, int alCount,
                                    eVertexBufferDrawType aDrawType = eVertexBufferDrawType_LastEnum) override;

        virtual void Bind() override;
        virtual void UnBind() override;

        virtual iVertexBuffer* CreateCopy(eVertexBufferType aType, eVertexBufferUsageType aUsageType, tVertexElementFlag alVtxToCopy) override;

        virtual cBoundingVolume CreateBoundingVolume() override;

        virtual int GetVertexNum() override;
        virtual int GetIndexNum() override;

        virtual int GetElementNum(eVertexBufferElement aElement) override;
        virtual eVertexBufferElementFormat GetElementFormat(eVertexBufferElement aElement) override;
        virtual int GetElementProgramVarIndex(eVertexBufferElement aElement) override;

        virtual float* GetFloatArray(eVertexBufferElement aElement) override;
        virtual int* GetIntArray(eVertexBufferElement aElement) override;
        virtual unsigned char* GetByteArray(eVertexBufferElement aElement) override;

        virtual unsigned int* GetIndices() override;

        virtual void ResizeArray(eVertexBufferElement aElement, int alSize) override;
        virtual void ResizeIndices(int alSize) override;

    protected:
        absl::InlinedVector<VertexElement, 10> m_vertexElements = {};
        std::vector<uint32_t> m_indices = {};
        bgfx::DynamicIndexBufferHandle m_dynamicIndexHandle = BGFX_INVALID_HANDLE;
    };

}; // namespace hpl
