#pragma once

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "graphics/GraphicsTypes.h"
#include "math/MathTypes.h"
#include "math/MeshTypes.h"
#include "physics/PhysicsTypes.h"
#include "system/SystemTypes.h"

#include <cstdint>
#include <optional>
#include <span>

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "Common_3/Utilities/Math/MathTypes.h"
#include "FixPreprocessor.h"

namespace hpl {

    class AssetBuffer final {
    public:
        enum class IndexType : uint8_t { Uint16, Uint32 };

        AssetBuffer() {}
        explicit AssetBuffer(const AssetBuffer&);
        explicit AssetBuffer(AssetBuffer&&);
        explicit AssetBuffer(uint32_t numBytes);

        void operator=(const AssetBuffer& other);
        void operator=(AssetBuffer&& other);

        class BufferRawView {
        public:
            BufferRawView() {
            }
            BufferRawView(AssetBuffer* asset, uint32_t byteOffset) :
                m_asset(asset),
                m_byteOffset(byteOffset) {
                ASSERT(m_byteOffset <= m_asset->Data().size());
            }
            BufferRawView(const BufferRawView& view):
                m_asset(view.m_asset),
                m_byteOffset(view.m_byteOffset) {
            }
            BufferRawView(BufferRawView&& view) {
                m_asset = view.m_asset;
                m_byteOffset = view.m_byteOffset;
            }

            void InsertBytes(uint32_t byteOffset, std::span<uint8_t> data) {
                for(size_t i = 0; i < data.size(); i++) {

                    m_asset->m_buffer.insert(
                        m_asset->m_buffer.begin() + byteOffset, data.begin(), data.end());
                }
            }

            void operator=(const BufferRawView& other) {
                m_asset = other.m_asset;
                m_byteOffset = other.m_byteOffset;
            }

            void operator=(BufferRawView&& other)  {
                m_asset = other.m_asset;
                m_byteOffset = other.m_byteOffset;
            }

            inline std::span<uint8_t> RawView() { return m_asset->m_buffer; }
            inline uint32_t NumBytes() { return  (m_asset->m_buffer.size() - m_byteOffset); }
        protected:
            AssetBuffer* m_asset = nullptr;
            uint32_t m_byteOffset = 0;
        };

        struct BufferIndexView: BufferRawView {
        public:
            BufferIndexView(AssetBuffer* asset, uint32_t byteOffset, IndexType type)
                : BufferRawView(asset, byteOffset)
                , m_indexType(type) {
            }

            BufferIndexView(const BufferIndexView& view):
                BufferRawView(view),
                m_indexType(view.m_indexType) {
            }

            BufferIndexView(const BufferIndexView&& view):
                BufferRawView(std::move(view)),
                m_indexType(view.m_indexType) {

            }
            void operator= (BufferIndexView&& other) {
                BufferRawView::operator=(std::move(other));
                m_indexType = other.m_indexType;
            }
            void operator= (const BufferIndexView& other) {
                BufferRawView::operator=(other);
                m_indexType = other.m_indexType;
            }


            void ResizeElements(uint32_t numElements) {

                switch(m_indexType) {
                    case IndexType::Uint32:
                        m_asset->m_buffer.resize(m_byteOffset + (numElements * sizeof(uint32_t)));
                        break;
                    case IndexType::Uint16:
                        m_asset->m_buffer.resize(m_byteOffset + (numElements * sizeof(uint16_t)));
                        break;
                }
            }

            void ReserveElements(uint32_t numElements) {
                switch(m_indexType) {
                    case IndexType::Uint32:
                        m_asset->m_buffer.reserve(m_byteOffset + (numElements * sizeof(uint32_t)));
                        break;
                    case IndexType::Uint16:
                        m_asset->m_buffer.reserve(m_byteOffset + (numElements * sizeof(uint16_t)));
                        break;
                }
            }

            uint32_t Get(uint32_t index) {
                switch(m_indexType) {
                    case IndexType::Uint32:
                        return *reinterpret_cast<uint32_t*>(m_asset->m_buffer.data() + (m_byteOffset + (index * sizeof(uint32_t))));
                    case IndexType::Uint16:
                        return *reinterpret_cast<uint16_t*>(m_asset->m_buffer.data() + (m_byteOffset + (index * sizeof(uint16_t))));
                }
                return UINT32_MAX;
            }
            void Insert(uint32_t index, uint32_t value) {
                switch(m_indexType) {
                    case IndexType::Uint32: {
                        uint16_t v = value;
                        auto buf = std::span(reinterpret_cast<const uint8_t*>(&v), sizeof(uint16_t));
                        m_asset->m_buffer.insert(
                            m_asset->m_buffer.begin() +
                            (m_byteOffset + (index * sizeof(uint32_t))), buf.begin(), buf.end());
                        break;
                    }
                    case IndexType::Uint16: {
                        uint16_t v = value;
                        auto buf = std::span(reinterpret_cast<const uint8_t*>(&v), sizeof(uint16_t));
                        m_asset->m_buffer.insert(
                            m_asset->m_buffer.begin() +
                            (m_byteOffset + (index * sizeof(uint16_t))), buf.begin(), buf.end());
                        break;
                    }
                }
            }

        private:
           IndexType m_indexType = IndexType::Uint32;
        };

        template<typename T>
        struct BufferStructuredView: BufferRawView {
        public:

            BufferStructuredView() {}
            BufferStructuredView(AssetBuffer* asset, uint32_t byteOffset, uint32_t byteStride)
                : BufferRawView(asset, byteOffset)
                , m_byteStride(byteStride) {
                ASSERT(m_byteStride >= sizeof(T));
            }

            BufferStructuredView(BufferStructuredView<T>&& view):
                BufferRawView(std::move(view)),
                m_byteStride(view.m_byteStride)
                {}
            BufferStructuredView(const BufferStructuredView<T>& view):
                BufferRawView(view),
                m_byteStride(view.m_byteStride)
                {}

            void operator=(BufferStructuredView<T>& view) {
                BufferRawView::operator=(view);
                m_byteStride = view.m_byteStride;
            }

            void operator=(BufferStructuredView<T>&& view) {
                BufferRawView::operator=(std::move(view));
                m_byteStride = view.m_byteStride;
            }

            inline uint32_t NumElements() {
                return NumBytes() / m_byteStride;
            }

            void Insert(uint32_t index, const T& value) {
                auto buf = std::span(reinterpret_cast<const uint8_t*>(&value), sizeof(T));
                m_asset->m_buffer.insert(
                    m_asset->m_buffer.begin() +
                    (m_byteOffset + (index * m_byteStride)), buf.begin(), buf.end());
            }

            void Insert(uint32_t index, const std::span<T> values) {
                auto buf = std::span<uint8_t>(
                    reinterpret_cast<const uint8_t*>(values.data()),
                    values.size() * sizeof(T));
                m_asset->m_buffer.insert(
                    m_asset->m_buffer.begin() +
                    (m_byteOffset + (index * m_byteStride)), buf.begin(), buf.end());
            }

            void ResizeElements(uint32_t numElements) {
                m_asset->m_buffer.resize(m_byteOffset + (numElements * m_byteStride));
            }

            void ReserveElements(uint32_t numElements) {
                m_asset->m_buffer.reserve(m_byteOffset + (numElements * m_byteStride));
            }

            T Get(uint32_t index) {
                return *reinterpret_cast<T*>(m_asset->m_buffer.data() + (m_byteOffset + (index * m_byteStride)));
            }

            std::span<T> View() {
                ASSERT(m_byteStride == sizeof(T));
                return std::span<T>(reinterpret_cast<T*>(m_asset->m_buffer.data() + m_byteOffset), NumBytes() / sizeof(T));
            }
        protected:
            uint32_t m_byteStride;
        };

        template<typename Type>
        constexpr BufferStructuredView<Type> CreateStructuredView(uint32_t byteOffset = 0, uint32_t byteStride = 0) {
            return BufferStructuredView<Type>(this, byteOffset, (byteStride == 0) ? sizeof(Type) : byteStride);
        }

        BufferIndexView CreateIndexView(uint32_t byteOffset = 0, IndexType type = IndexType::Uint32) {
            return BufferIndexView(this, byteOffset, type);
        }

        BufferRawView CreateViewRaw(uint32_t byteOffset = 0) {
            return BufferRawView(this, byteOffset);
        }

        std::span<uint8_t> Data();
    private:
        std::vector<uint8_t> m_buffer;
    };


} // namespace hpl
