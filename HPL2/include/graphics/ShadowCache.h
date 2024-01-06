#pragma once

#include "graphics/IndexPool.h"
#include <cstdint>
#include <optional>
#include <vector>

#include <folly/small_vector.h>

#include <Common_3/Graphics/Interfaces/IGraphics.h>
#include <Common_3/Utilities/RingBuffer.h>
#include <FixPreprocessor.h>

namespace hpl {
    template<typename MetaInfo>
    class ShadowCache {
    public:
        static constexpr uint32_t MinimumAge = 3;
        static constexpr uint4 InvalidQuad = uint4(0, 0, 0, 0);
        struct ShadowSlot {
            uint32_t m_age = 0;
            MetaInfo m_info = {};
        };

        struct ShadowContainer {
        public:
            uint16_t m_x = 0;
            uint16_t m_y = 0;
            uint8_t m_level = 0;
            uint32_t m_age = 0;
            folly::small_vector<ShadowSlot, 24> m_slots;
        };

        ShadowCache() = default;
        ShadowCache(uint32_t width, uint32_t height, uint32_t numColumns, uint32_t numRows, uint8_t maxLevels)
            : m_maxLevels(maxLevels)
            , m_numRows(numRows)
            , m_numColumns(numColumns)
            , m_size(width, height)
            , m_quadSize(width / numColumns, height / numRows) {
            m_containers.resize(numColumns * numRows);
            for(size_t i = 0; i < m_containers.size(); i++) {
                m_containers[i].m_slots.resize(1);
                m_containers[i].m_x = m_quadSize.x * (i % numColumns);
                m_containers[i].m_y = m_quadSize.y * (i / numColumns);
            }

        }
        struct ShadowMapResult {
        public:
            ShadowMapResult(ShadowCache<MetaInfo>* cache, ShadowContainer* container, uint32_t slotIdx, uint32_t age)
                : m_cache(cache)
                , m_container(container)
                , m_slotIdx(slotIdx)
                , m_age(age) {
            }
            // mark shadowmap as used
            void Mark() {
                m_container->m_age = m_age;
                m_container->m_slots[m_slotIdx].m_age = m_age;
            }

            inline float4 NormalizedRect() {
                uint4 quad = Rect();
                return float4(
                    static_cast<float>(quad.x) / static_cast<float>(m_cache->m_size.x),
                    static_cast<float>(quad.y) / static_cast<float>(m_cache->m_size.y),
                    static_cast<float>(quad.z) / static_cast<float>(m_cache->m_size.x),
                    static_cast<float>(quad.w) / static_cast<float>(m_cache->m_size.y));
            }

            inline MetaInfo& Meta() {
                return m_container->m_slots[m_slotIdx].m_info;
            }

            inline uint4 Rect() {
                const uint8_t div = (1 << m_container->m_level);
                const uint2 size = (m_cache->m_quadSize / div);
                return uint4(
                    m_container->m_x + ((m_slotIdx % div) * size.x),
                    m_container->m_y + ((m_slotIdx / div) * size.y),
                    size.x, size.y);
            }

        private:
            ShadowCache<MetaInfo>* m_cache;
            ShadowContainer* m_container;
            size_t m_slotIdx;
            uint32_t m_age;
        };

        std::optional<ShadowMapResult> Search(uint8_t targetLevel, uint32_t age, std::function<bool(MetaInfo&)> foundHandler) {
            uint8_t level = std::min(m_maxLevels, targetLevel);
            const uint8_t divisions = (1 << level);
            uint2 shadowSize = m_quadSize / divisions;
            struct Candidate {
                ShadowContainer* m_container;
                uint32_t m_slotIdx;
            };

            Candidate bestCandidate{};
            ShadowContainer* replaceCandidate = nullptr;
            for (auto& container : m_containers) {
                if (container.m_level == level) {
                    for (size_t i = 0; i < (divisions * divisions); i++) {
                        auto& slot = container.m_slots[i];
                        if (foundHandler(slot.m_info)) {
                            return ShadowMapResult(this, &container, i, age);
                        }

                        if (age - container.m_age > MinimumAge &&
                            (bestCandidate.m_container == nullptr ||
                            bestCandidate.m_container->m_slots[bestCandidate.m_slotIdx].m_age < slot.m_age)) {
                            bestCandidate.m_container = &container;
                            bestCandidate.m_slotIdx = i;
                        }
                    }
                } else {
                    if (age - container.m_age > MinimumAge && (replaceCandidate == nullptr || replaceCandidate->m_age < container.m_age)) {
                        replaceCandidate = &container;
                    }
                }
            }
            if (bestCandidate.m_container != nullptr) {
                return ShadowMapResult(this, bestCandidate.m_container, bestCandidate.m_slotIdx, age);
            }

            if (replaceCandidate) {
                replaceCandidate->m_level = level;
                replaceCandidate->m_slots.clear();
                replaceCandidate->m_slots.resize(divisions * divisions);
                return ShadowMapResult(this, replaceCandidate, 0, age);
            }

            return std::nullopt;
        }

        inline uint2 quadSize() {
            return m_quadSize;
        }
        inline uint2 size() {
            return m_size;
        }
        inline uint8_t maxLevels() {
            return m_maxLevels;
        }

    private:
        uint8_t m_maxLevels;
        uint16_t m_numRows;
        uint16_t m_numColumns;
        uint2 m_size;
        uint2 m_quadSize;
        std::vector<ShadowContainer> m_containers;
    };
} // namespace hpl
