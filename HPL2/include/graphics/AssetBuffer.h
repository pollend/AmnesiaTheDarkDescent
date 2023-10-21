#pragma once

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "graphics/GraphicsTypes.h"
#include "math/MathTypes.h"
#include "math/MeshTypes.h"
#include "physics/PhysicsTypes.h"
#include "system/SystemTypes.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "Common_3/Utilities/Math/MathTypes.h"
#include "FixPreprocessor.h"

namespace hpl {
    namespace details {
        inline void testAndResize(std::vector<uint8_t>& data, size_t reqSize) {
            if(reqSize > data.size()) {
                data.resize(reqSize);
            }
        }
    }


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

            /**
            * When Writing a raw to a buffer the buffer will resize to accomidate
            */
            void WriteRaw(uint32_t byteOffset, std::span<uint8_t> data) {
                details::testAndResize(m_asset->m_buffer, m_byteOffset + byteOffset + data.size());
                std::copy(data.begin(), data.end(), m_asset->m_buffer.begin() + m_byteOffset + byteOffset);
            }

            template<typename T>
            void WriteRawType(uint32_t byteOffset, const T& data) {
                std::span<uint8_t> raw(reinterpret_cast<uint8_t*>(&data), sizeof(T));
                details::testAndResize(m_asset->m_buffer, m_byteOffset + byteOffset + raw.size());
                std::copy(raw.begin(), raw.end(), m_asset->m_buffer.begin() + m_byteOffset + byteOffset);
            }

            template<typename T>
            void WriteRawType(uint32_t byteOffset, const std::span<T>& data) {
                std::span<const uint8_t> raw(reinterpret_cast<const uint8_t*>(data.data()), data.size() * sizeof(T));
                details::testAndResize(m_asset->m_buffer, m_byteOffset + byteOffset + raw.size());
                std::copy(raw.begin(), raw.end(), m_asset->m_buffer.begin() + m_byteOffset + byteOffset);
            }

            void operator=(const BufferRawView& other) {
                m_asset = other.m_asset;
                m_byteOffset = other.m_byteOffset;
            }

            void operator=(BufferRawView&& other)  {
                m_asset = other.m_asset;
                m_byteOffset = other.m_byteOffset;
            }

            template<typename T>
            inline std::span<T> RawDataView() { return std::span<T>(reinterpret_cast<T*>(m_asset->m_buffer.data() + m_byteOffset), (m_asset->m_buffer.size() - m_byteOffset) / sizeof(T));}
            inline std::span<uint8_t> RawView() { return std::span(m_asset->m_buffer.data() + m_byteOffset, m_asset->m_buffer.size() - m_byteOffset); }
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
                        ASSERT(m_asset->m_buffer.begin() + (m_byteOffset + (index * sizeof(uint32_t))) < m_asset->m_buffer.end());
                        return *reinterpret_cast<uint32_t*>(m_asset->m_buffer.data() + (m_byteOffset + (index * sizeof(uint32_t))));
                    case IndexType::Uint16:
                        ASSERT(m_asset->m_buffer.begin() + (m_byteOffset + (index * sizeof(uint16_t))) < m_asset->m_buffer.end());
                        return *reinterpret_cast<uint16_t*>(m_asset->m_buffer.data() + (m_byteOffset + (index * sizeof(uint16_t))));
                }
                return UINT32_MAX;
            }
            void Write(uint32_t index, uint32_t value) {
                switch(m_indexType) {
                    case IndexType::Uint32: {
                        uint32_t v = value;
                        auto buf = std::span(reinterpret_cast<const uint8_t*>(&v), sizeof(uint32_t));
                        const size_t targetOffset = (m_byteOffset + (index * sizeof(uint32_t)));
                        details::testAndResize(m_asset->m_buffer, targetOffset + buf.size());
                        std::copy(buf.begin(), buf.end(), m_asset->m_buffer.begin() + targetOffset);
                        break;
                    }
                    case IndexType::Uint16: {
                        uint16_t v = value;
                        auto buf = std::span(reinterpret_cast<const uint8_t*>(&v), sizeof(uint16_t));
                        const size_t targetOffset = (m_byteOffset + (index * sizeof(uint16_t)));
                        details::testAndResize(m_asset->m_buffer, targetOffset + buf.size());
                        std::copy(buf.begin(), buf.end(), m_asset->m_buffer.begin() + targetOffset);
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

            void Write(uint32_t index, const T& value) {
                auto buf = std::span(reinterpret_cast<const uint8_t*>(&value), sizeof(T));
                const size_t targetOffset = (m_byteOffset + (index * m_byteStride));
                details::testAndResize(m_asset->m_buffer, targetOffset + buf.size());
                std::copy(buf.begin(), buf.end(), m_asset->m_buffer.begin() + targetOffset);
            }

            void Write(uint32_t index, const std::span<T> values) {
                auto buf = std::span<uint8_t>(reinterpret_cast<const uint8_t*>(values.data()), values.size() * sizeof(T));
                const size_t targetOffset = (m_byteOffset + (index * m_byteStride));
                details::testAndResize(m_asset->m_buffer, targetOffset + buf.size());
                std::copy(buf.begin(), buf.end(), m_asset->m_buffer.begin() + targetOffset);
            }

            void ResizeElements(uint32_t numElements) {
                m_asset->m_buffer.resize(m_byteOffset + (numElements * m_byteStride));
            }

            void ReserveElements(uint32_t numElements) {
                m_asset->m_buffer.reserve(m_byteOffset + (numElements * m_byteStride));
            }

            T Get(uint32_t index) {
                ASSERT(m_asset->m_buffer.begin() + (m_byteOffset + (index * m_byteStride)) < m_asset->m_buffer.end());
                return *reinterpret_cast<T*>(m_asset->m_buffer.data() + (m_byteOffset + (index * m_byteStride)));
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
