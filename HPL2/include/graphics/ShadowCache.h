#pragma once

#include "graphics/IndexPool.h"
#include <cstdint>
#include <vector>

#include <folly/small_vector.h>

#include <Common_3/Graphics/Interfaces/IGraphics.h>
#include <Common_3/Utilities/RingBuffer.h>
#include <FixPreprocessor.h>


namespace hpl {
    class ShadowCache {
    public:

        struct ShadowGroup {
        public:
            static constexpr uint8_t IsLeaf = 0x1;
            static constexpr uint8_t HasLeafs = 0x2;
            static constexpr uint8_t HasGroups = 0x4;

            uint16_t m_idx;

            uint16_t m_x;
            uint16_t m_y;
            uint32_t m_age;

            uint16_t m_parent; // the parent of the cache entry
            uint16_t m_child; // the start of the group

            // linked list for all cut levels at the same cut
            uint16_t m_next;
            uint16_t m_prev;

            uint8_t m_flags;
            uint8_t m_occupyMask: 4; // mask for the entires occuped
            uint8_t m_slot: 4; // the slot index
            uint8_t m_level;

            uint16_t m_hashes[4];
        };


        struct CutLevel {
            uint16_t m_begin = UINT16_MAX;
        };

        ShadowCache(uint32_t width, uint32_t height, uint32_t hDivisions, uint32_t vDivision, uint8_t maxCuts);
        void reset(uint32_t age);
        uint4 find(uint16_t hash, uint8_t level, uint32_t age);
    private:
        ShadowGroup* allocGroup();
        ShadowCache::ShadowGroup* createCut(ShadowGroup* group);

        void freeGroup(ShadowGroup* group);
        void freeChildren(ShadowGroup* group);

        void updateAge(ShadowCache::ShadowGroup* current, uint32_t age);
        void insertAfter(ShadowCache::ShadowGroup* current, ShadowCache::ShadowGroup* grp);
        void insertBefore(ShadowCache::ShadowGroup* current, ShadowCache::ShadowGroup* grp);

        uint2 m_quadSize;
        hpl::IndexPool m_pool;
        std::vector<ShadowGroup> m_alloc; // a reserved pool of entries will contain the max possible i.e all the quads are at its lowest cut
        std::vector<CutLevel> m_cut; // cut levels for each quad
    };
}
