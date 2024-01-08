#pragma once

#include <cstdint>

namespace hpl::graphics {

    struct RangeSubsetAlloc {
    public:
        struct RangeSubset {
            uint32_t m_start = 0;
            uint32_t m_end = 0;
            inline uint32_t size() {
                return m_end - m_start;
            }
        };
        RangeSubsetAlloc(uint32_t& index)
            : m_index(index)
            , m_start(index) {
        }
        uint32_t Increment() {
            return (m_index++);
        }
        RangeSubset End() {
            uint32_t start = m_start;
            m_start = m_index;
            return RangeSubset{ start, m_index };
        }

    private:
        uint32_t& m_index;
        uint32_t m_start;
    };

} // namespace hpl::graphics
