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

#include <graphics/GraphicsContext.h>
#include <graphics/VertexBuffer.h>

#include <absl/container/inlined_vector.h>
#include <algorithm>
#include <array>
#include <bgfx/bgfx.h>
#include <bx/debug.h>
#include <span>
#include <vector>

#include <Common_3/Graphics/Interfaces/IGraphics.h>
#include <FixPreprocessor.h>

namespace hpl {

    class LegacyVertexBuffer : public iVertexBuffer {
    public:
        static size_t GetSizeFromHPL(eVertexBufferElementFormat format);
        [[deprecated("removing bgfx dependency")]] static bgfx::Attrib::Enum GetAttribFromHPL(eVertexBufferElement type);
        [[deprecated("removing bgfx dependency")]] static bgfx::AttribType::Enum GetAttribTypeFromHPL(eVertexBufferElementFormat format);

        // static constexpr std::array

        struct VertexElement {
            ForgeBufferHandle m_buffer;
            eVertexBufferElementFormat m_format = eVertexBufferElementFormat::eVertexBufferElementFormat_Float;
            eVertexBufferElement m_type = eVertexBufferElement::eVertexBufferElement_Position;
            tVertexElementFlag m_flag = 0;
            size_t m_num = 0;
            int m_programVarIndex = 0; // for legacy behavior

            size_t Stride() const;
            size_t NumElements() const;

            template<typename TData>
            std::span<TData> GetElements() {
                ASSERT(sizeof(TData) == Stride() && "Data must be same size as stride");
                return std::span<TData*>(reinterpret_cast<TData*>(m_shadowData.data()), m_shadowData.size() / Stride());
            }

            std::span<uint8_t> Data() const {
                return m_shadowData;
            }

            template<typename TData>
            TData& GetElement(size_t index) {
                ASSERT(sizeof(TData) <= Stride() && "Date must be less than or equal to stride");
                return *reinterpret_cast<TData*>(m_shadowData.data() + index * Stride());
            }

        private:
            mutable size_t m_activeCopy = 0; // the active copy of the data 
            mutable size_t m_internalBufferSize = 0; // the size of the internal buffer
            mutable std::vector<uint8_t> m_shadowData = {};
            friend class LegacyVertexBuffer;
        };

        LegacyVertexBuffer(eVertexBufferDrawType aDrawType, eVertexBufferUsageType aUsageType, int alReserveVtxSize, int alReserveIdxSize);
        ~LegacyVertexBuffer();

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
        virtual void DrawIndices(
            unsigned int* apIndices, int alCount, eVertexBufferDrawType aDrawType = eVertexBufferDrawType_LastEnum) override;
        // virtual void Submit(GraphicsContext& context, eVertexBufferDrawType aDrawType = eVertexBufferDrawType_LastEnum) override;
        virtual void GetLayoutStream(
            GraphicsContext::LayoutStream& layoutStream, eVertexBufferDrawType aDrawType = eVertexBufferDrawType_LastEnum) override;

        struct GeometryBinding {
            struct VertexGeometryEntry {
                VertexElement* element;
                uint64_t offset;
            };
            struct VertexIndexEntry {
                ForgeBufferHandle* element;
                uint64_t offset;
                uint32_t numIndicies;
            };
            absl::InlinedVector<VertexGeometryEntry, eVertexBufferElement_LastEnum>
                m_vertexElement; // elements are in the order they are requested
            VertexIndexEntry m_indexBuffer;
        };
        void resolveGeometryBinding(
            uint32_t currentFrame, std::span<eVertexBufferElement> elements, GeometryBinding* binding);

        // virtual void Bind() override;
        virtual void UnBind() override;

        virtual iVertexBuffer* CreateCopy(
            eVertexBufferType aType, eVertexBufferUsageType aUsageType, tVertexElementFlag alVtxToCopy) override;

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

        const VertexElement* GetElement(eVertexBufferElement elementType);

    protected:
        // size_t m_bufferIndex = 0;
        
        static void PushVertexElements(
            std::span<const float> values, eVertexBufferElement elementType, std::span<LegacyVertexBuffer::VertexElement> elements);

        absl::InlinedVector<VertexElement, 10> m_vertexElements = {};
        ForgeBufferHandle m_indexBuffer;
        std::vector<uint32_t> m_indices = {};

        size_t m_indexBufferActiveCopy = 0;
        tVertexElementFlag m_updateFlags = 0; // update no need to rebuild buffers
        bool m_updateIndices = false;

        friend struct VertexElement;
    };

}; // namespace hpl
