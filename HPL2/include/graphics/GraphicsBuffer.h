#pragma once

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

#include "graphics/GraphicsTypes.h"
#include "graphics/Enum.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"
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

    class GraphicsBuffer final {
    public:
        enum BufferType {
            ResourceBuffer,
            MappedBuffer
        };
        struct MappedBufferDesc {
            uint64_t m_size;
            void* m_mappedData;
        };

        GraphicsBuffer() {}
        explicit GraphicsBuffer(const BufferUpdateDesc& desc);
        explicit GraphicsBuffer(const MappedBufferDesc& desc);
        explicit GraphicsBuffer(const GraphicsBuffer&);
        explicit GraphicsBuffer(GraphicsBuffer&&);

        void operator=(const GraphicsBuffer& other);
        void operator=(GraphicsBuffer&& other);

        class BufferRawView {
        public:
            BufferRawView() {
            }
            BufferRawView(GraphicsBuffer* asset, uint32_t byteOffset) :
                m_asset(asset),
                m_byteOffset(byteOffset) {
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
                switch(m_asset->m_type) {
                    case BufferType::ResourceBuffer: {
                        details::testAndResize(m_asset->m_buffer, m_byteOffset + byteOffset + data.size());
                        std::copy(data.begin(), data.end(), m_asset->m_buffer.begin() + m_byteOffset + byteOffset);
                        break;
                    }
                    case BufferType::MappedBuffer: {
                        ASSERT(m_asset->m_mapped.m_size == 0 || m_byteOffset + byteOffset + data.size() < m_asset->m_mapped.m_size);
                        std::copy(data.begin(), data.end(), reinterpret_cast<uint8_t*>(m_asset->m_mapped.m_mappedData) + m_byteOffset + byteOffset);
                        break;
                    }
                }
            }

            template<typename T>
            void WriteRawType(uint32_t byteOffset, const T& data) {
                std::span<uint8_t> raw(reinterpret_cast<uint8_t*>(&data), sizeof(T));
                switch(m_asset->m_type) {
                    case BufferType::ResourceBuffer:
                        details::testAndResize(m_asset->m_buffer, m_byteOffset + byteOffset + raw.size());
                        std::copy(raw.begin(), raw.end(), m_asset->m_buffer.begin() + m_byteOffset + byteOffset);
                        break;
                    case BufferType::MappedBuffer:
                        std::copy(raw.begin(), raw.end(), reinterpret_cast<uint8_t*>(m_asset->m_mapped.m_mappedData) + m_byteOffset + byteOffset);
                        break;
                }
            }

            template<typename T>
            void WriteRawType(uint32_t byteOffset, const std::span<T>& data) {
                std::span<const uint8_t> raw(reinterpret_cast<const uint8_t*>(data.data()), data.size() * sizeof(T));
                switch(m_asset->m_type) {
                    case BufferType::ResourceBuffer:
                        details::testAndResize(m_asset->m_buffer, m_byteOffset + byteOffset + raw.size());
                        std::copy(raw.begin(), raw.end(), m_asset->m_buffer.begin() + m_byteOffset + byteOffset);
                        break;
                    case BufferType::MappedBuffer:
                        ASSERT(m_asset->m_mapped.m_mappedData);
                        std::copy(raw.begin(), raw.end(), reinterpret_cast<uint8_t*>(m_asset->m_mapped.m_mappedData) + m_byteOffset + byteOffset);
                        break;
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

            template<typename T>
            inline std::span<T> rawSpanByType() {
                ASSERT(m_asset->m_type == BufferType::ResourceBuffer);
                switch(m_asset->m_type) {
                    case BufferType::ResourceBuffer:
                        return std::span<T>(reinterpret_cast<T*>(m_asset->m_buffer.data() + m_byteOffset), (m_asset->m_buffer.size() - m_byteOffset) / sizeof(T));
                    case BufferType::MappedBuffer: {
                        ASSERT(m_asset->m_mapped.m_size > 0); // can't have a span over data with a unbounded buffer
                        return std::span<T>(reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(m_asset->m_mapped.m_mappedData) + m_byteOffset), (m_asset->m_mapped.m_size - m_byteOffset) / sizeof(T));
                    }
                }
            }
            inline std::span<uint8_t> rawByteSpan() {
                switch(m_asset->m_type) {
                    case BufferType::ResourceBuffer:
                        return std::span(m_asset->m_buffer.data() + m_byteOffset, m_asset->m_buffer.size() - m_byteOffset);
                    case BufferType::MappedBuffer:
                        ASSERT(m_asset->m_mapped.m_size > 0); // can't have a span over data with a unbounded buffer
                        return std::span<uint8_t>(reinterpret_cast<uint8_t*>(m_asset->m_mapped.m_mappedData) + m_byteOffset, m_asset->m_mapped.m_size - m_byteOffset);
                }
                return std::span<uint8_t>();
            }
            inline uint32_t NumBytes() {
                switch(m_asset->m_type) {
                    case BufferType::ResourceBuffer:
                        return  (m_asset->m_buffer.size() - m_byteOffset);
                    case BufferType::MappedBuffer:
                        return (m_asset->m_mapped.m_size == 0) ? UINT32_MAX : m_asset->m_mapped.m_size - m_byteOffset;
                }
                return UINT32_MAX;
            }
        protected:
            GraphicsBuffer* m_asset = nullptr;
            uint32_t m_byteOffset = 0;
        };

        struct BufferIndexView final: BufferRawView {
        public:
            BufferIndexView(GraphicsBuffer* asset, uint32_t byteOffset, IndexBufferType type)
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
                switch(m_asset->m_type) {
                    case BufferType::ResourceBuffer: {
                        switch(m_indexType) {
                            case IndexBufferType::Uint32:
                                m_asset->m_buffer.resize(m_byteOffset + (numElements * sizeof(uint32_t)));
                                break;
                            case IndexBufferType::Uint16:
                                m_asset->m_buffer.resize(m_byteOffset + (numElements * sizeof(uint16_t)));
                                break;
                        }
                        break;
                    }
                    case BufferType::MappedBuffer:
                        ASSERT(false); // you can't resize a mapped buffer
                        break;
                }
            }

            void ReserveElements(uint32_t numElements) {
                switch(m_asset->m_type) {
                    case BufferType::ResourceBuffer: {
                        switch(m_indexType) {
                            case IndexBufferType::Uint32:
                                m_asset->m_buffer.reserve(m_byteOffset + (numElements * sizeof(uint32_t)));
                                break;
                            case IndexBufferType::Uint16:
                                m_asset->m_buffer.reserve(m_byteOffset + (numElements * sizeof(uint16_t)));
                                break;
                        }
                        break;
                    }
                    case BufferType::MappedBuffer:
                        ASSERT(false);
                        break;
                }
            }

            uint32_t Get(uint32_t index) {
                switch(m_asset->m_type) {
                    case BufferType::ResourceBuffer: {
                        switch(m_indexType) {
                            case IndexBufferType::Uint32:
                                ASSERT(m_asset->m_buffer.begin() + (m_byteOffset + (index * sizeof(uint32_t))) < m_asset->m_buffer.end());
                                return *reinterpret_cast<uint32_t*>(m_asset->m_buffer.data() + (m_byteOffset + (index * sizeof(uint32_t))));
                            case IndexBufferType::Uint16:
                                ASSERT(m_asset->m_buffer.begin() + (m_byteOffset + (index * sizeof(uint16_t))) < m_asset->m_buffer.end());
                                return *reinterpret_cast<uint16_t*>(m_asset->m_buffer.data() + (m_byteOffset + (index * sizeof(uint16_t))));
                        }
                    }
                    case BufferType::MappedBuffer:
                        switch(m_indexType) {
                            case IndexBufferType::Uint32:
                                ASSERT(m_asset->m_buffer.begin() + (m_byteOffset + (index * sizeof(uint32_t))) < m_asset->m_buffer.end());
                                return *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(m_asset->m_mapped.m_mappedData) + (m_byteOffset + (index * sizeof(uint32_t))));
                            case IndexBufferType::Uint16:
                                ASSERT(m_asset->m_buffer.begin() + (m_byteOffset + (index * sizeof(uint16_t))) < m_asset->m_buffer.end());
                                return *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(m_asset->m_mapped.m_mappedData) + (m_byteOffset + (index * sizeof(uint16_t))));
                        }
                        break;
                }
                return UINT32_MAX;
            }
            void Write(uint32_t index, uint32_t value) {
                switch(m_asset->m_type) {
                    case BufferType::ResourceBuffer: {
                        switch(m_indexType) {
                            case IndexBufferType::Uint32: {
                                uint32_t v = value;
                                auto buf = std::span(reinterpret_cast<const uint8_t*>(&v), sizeof(uint32_t));
                                const size_t targetOffset = (m_byteOffset + (index * sizeof(uint32_t)));
                                details::testAndResize(m_asset->m_buffer, targetOffset + buf.size());
                                std::copy(buf.begin(), buf.end(), m_asset->m_buffer.begin() + targetOffset);
                                break;
                            }
                            case IndexBufferType::Uint16: {
                                uint16_t v = value;
                                auto buf = std::span(reinterpret_cast<const uint8_t*>(&v), sizeof(uint16_t));
                                const size_t targetOffset = (m_byteOffset + (index * sizeof(uint16_t)));
                                details::testAndResize(m_asset->m_buffer, targetOffset + buf.size());
                                std::copy(buf.begin(), buf.end(), m_asset->m_buffer.begin() + targetOffset);
                                break;
                            }
                        }
                        break;
                    }
                    case BufferType::MappedBuffer: {

                        switch(m_indexType) {
                            case IndexBufferType::Uint32: {
                                uint32_t v = value;
                                auto buf = std::span(reinterpret_cast<const uint8_t*>(&v), sizeof(uint32_t));
                                const size_t targetOffset = (m_byteOffset + (index * sizeof(uint32_t)));
                                details::testAndResize(m_asset->m_buffer, targetOffset + buf.size());
                                std::copy(buf.begin(), buf.end(), reinterpret_cast<uint8_t*>(m_asset->m_mapped.m_mappedData) + targetOffset);
                                break;
                            }
                            case IndexBufferType::Uint16: {
                                uint16_t v = value;
                                auto buf = std::span(reinterpret_cast<const uint8_t*>(&v), sizeof(uint16_t));
                                const size_t targetOffset = (m_byteOffset + (index * sizeof(uint16_t)));
                                details::testAndResize(m_asset->m_buffer, targetOffset + buf.size());
                                std::copy(buf.begin(), buf.end(), reinterpret_cast<uint8_t*>(m_asset->m_mapped.m_mappedData) + targetOffset);
                                break;
                            }
                        }
                        break;
                    }
                }
            }

        private:
           IndexBufferType m_indexType = IndexBufferType::Uint32;
        };

        template<typename T>
        struct BufferStructuredView final: BufferRawView {
        public:

            BufferStructuredView() {}
            BufferStructuredView(GraphicsBuffer* asset, uint32_t byteOffset, uint32_t byteStride)
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
                switch(m_asset->m_type) {
                    case BufferType::ResourceBuffer: {
                        details::testAndResize(m_asset->m_buffer, targetOffset + buf.size());
                        std::copy(buf.begin(), buf.end(), m_asset->m_buffer.begin() + targetOffset);
                        break;
                    }
                    case BufferType::MappedBuffer: {
                        std::copy(buf.begin(), buf.end(), reinterpret_cast<uint8_t*>(m_asset->m_mapped.m_mappedData) + targetOffset);
                        break;
                    }
                }
            }

            void Write(uint32_t index, const std::span<T> values) {
                auto buf = std::span<uint8_t>(reinterpret_cast<const uint8_t*>(values.data()), values.size() * sizeof(T));
                const size_t targetOffset = (m_byteOffset + (index * m_byteStride));
                switch(m_asset->m_type) {
                    case BufferType::ResourceBuffer: {
                        details::testAndResize(m_asset->m_buffer, targetOffset + buf.size());
                        std::copy(buf.begin(), buf.end(), m_asset->m_buffer.begin() + targetOffset);
                        break;
                    }
                    case BufferType::MappedBuffer: {
                        std::copy(buf.begin(), buf.end(), reinterpret_cast<uint8_t*>(m_asset->m_mapped.m_mappedData) + targetOffset);
                        break;
                    }
                }
            }

            void ResizeElements(uint32_t numElements) {
                switch(m_asset->m_type) {
                    case BufferType::ResourceBuffer: {
                        m_asset->m_buffer.resize(m_byteOffset + (numElements * m_byteStride));
                        break;
                    }
                    case BufferType::MappedBuffer: {
                        ASSERT(false);
                        break;
                    }
                }
            }

            void ReserveElements(uint32_t numElements) {
                switch(m_asset->m_type) {
                    case BufferType::ResourceBuffer: {
                        m_asset->m_buffer.reserve(m_byteOffset + (numElements * m_byteStride));
                        break;
                    }
                    case BufferType::MappedBuffer: {
                        ASSERT(false);
                        break;
                    }
                }
            }

            T Get(uint32_t index) {
                switch(m_asset->m_type) {
                    case BufferType::ResourceBuffer: {
                        ASSERT(m_asset->m_buffer.begin() + (m_byteOffset + (index * m_byteStride)) < m_asset->m_buffer.end());
                        return *reinterpret_cast<T*>(m_asset->m_buffer.data() + (m_byteOffset + (index * m_byteStride)));
                    }
                    case BufferType::MappedBuffer: {
                        ASSERT(false);
                        break;
                    }
                }
                return T();
            }
        protected:
            uint32_t m_byteStride;
        };

        template<typename Type>
        constexpr BufferStructuredView<Type> CreateStructuredView(uint32_t byteOffset = 0, uint32_t byteStride = 0) {
            return BufferStructuredView<Type>(this, byteOffset, (byteStride == 0) ? sizeof(Type) : byteStride);
        }

        BufferIndexView CreateIndexView(uint32_t byteOffset = 0, IndexBufferType type = IndexBufferType::Uint32) {
            return BufferIndexView(this, byteOffset, type);
        }

        BufferRawView CreateViewRaw(uint32_t byteOffset = 0) {
            return BufferRawView(this, byteOffset);
        }
    private:
        struct {
            uint64_t m_size;
            void* m_mappedData;
        } m_mapped;
        std::vector<uint8_t> m_buffer;
        BufferType m_type = BufferType::ResourceBuffer;
    };


} // namespace hpl
