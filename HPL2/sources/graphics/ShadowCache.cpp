#include "graphics/ShadowCache.h"
#include "graphics/IndexPool.h"
#include <cstdint>
#include <cstring>
#include <limits>

namespace hpl {

    void ShadowCache::insertAfter(ShadowCache::ShadowGroup* current, ShadowCache::ShadowGroup* grp) {
        ASSERT(current);
        ASSERT(grp);
        uint16_t nextIdx = current->m_next;

        current->m_next = grp->m_idx;
        grp->m_prev = current->m_idx;

        grp->m_next = nextIdx;
        if(nextIdx != UINT16_MAX) {
            m_alloc[nextIdx].m_prev = grp->m_idx;
        }
    }
    void ShadowCache::insertBefore(ShadowCache::ShadowGroup* current, ShadowCache::ShadowGroup* grp) {
        ASSERT(current);
        ASSERT(grp);
        uint16_t prevIdx = current->m_prev;

        current->m_prev = grp->m_idx;
        grp->m_next = current->m_idx;

        grp->m_prev = prevIdx;
        if(prevIdx != UINT16_MAX) {
            m_alloc[prevIdx].m_next = grp->m_idx;
        }
    }
    ShadowCache::ShadowCache(uint32_t width, uint32_t height, uint32_t hDivisions, uint32_t vDivision, uint8_t maxCuts):
        m_quadSize(width/vDivision, height/ hDivisions),
        m_pool((std::pow(std::max(1, (static_cast<int>(maxCuts) - 2)), 4) + 1) * hDivisions * vDivision) {
        ASSERT(m_pool.reserve() > std::numeric_limits<uint16_t>::max());
        m_alloc.resize(m_pool.reserve());
        m_cut.resize(maxCuts);
        ShadowCache::ShadowGroup* previous = nullptr;
        for(size_t x = 0; x < hDivisions; x++) {
            for(size_t y = 0; y < vDivision; y++) {
                ShadowCache::ShadowGroup* entry = allocGroup();
                entry->m_x = m_quadSize.x * x;
                entry->m_y = m_quadSize.y * y;
                if(previous != nullptr) {
                    insertAfter(previous, entry);
                }
                if(previous == nullptr) {
                    m_cut[0].m_begin = entry->m_idx;
                }
                previous = entry;
            }
        }
    }

    ShadowCache::ShadowGroup* ShadowCache::createCut(ShadowGroup* group) {
        ShadowGroup* child = allocGroup();
        child->m_parent = group->m_idx;
        if (group->m_child == UINT16_MAX) {
            group->m_child = child->m_idx;
            child->m_parent = group->m_idx;
        } else {
            insertAfter(&m_alloc[group->m_child], child);
        }
        child->m_level = group->m_level + 1;
        bool found = false;
        for (uint8_t i = 0; i < 4; i++) {
            if ((group->m_occupyMask & (1 << i)) == 0) {
                group->m_occupyMask |= (1 << i); // we are going to set this entry is occupied
                uint16_t x = (i % 2);
                uint16_t y = (i / 2);
                child->m_slot = i; // we assign the slot we occupy for this cut
                uint2 quadSize = m_quadSize / (1 << child->m_level);
                group->m_x = group->m_x + (x * quadSize.x);
                group->m_y = group->m_y + (y * quadSize.y);
                group->m_flags |= ShadowGroup::HasGroups;
                found = true;
                break;
            }
        }
        ASSERT(found);
        auto& nextCut = m_cut[child->m_level];
        if (nextCut.m_begin == UINT16_MAX) {
            nextCut.m_begin = child->m_idx;
        } else {
            insertBefore(&m_alloc[nextCut.m_begin], group);
        }

        return child;
    }

    uint4 ShadowCache::find(uint16_t hash, uint8_t level, uint32_t age) {
        auto findGroupInCut = [&](ShadowGroup* group) {
            ShadowGroup* result = nullptr;
            for (uint16_t i = group->m_idx; i != UINT16_MAX; i = m_alloc[i].m_next) {
                for(size_t k = 0; k < 4; k++) {
                    // we found a matching hash
                    if(m_alloc[i].m_hashes[k] == hash) {
                        result = &m_alloc[i];
                        goto exit;
                    }
                }
                if (result == nullptr || m_alloc[i].m_age < result->m_age) {
                    result = &m_alloc[i];
                }
            }
        exit:
            return result;
        };
        const uint8_t targetLevel = level == 0? 0: level - 1;
        auto& targetCut = m_cut[targetLevel];

        ShadowGroup* group = findGroupInCut(&m_alloc[targetCut.m_begin]);
        if(level == 0) {
            freeChildren(group);
            group->m_flags |= ShadowGroup::IsLeaf;
            group->m_hashes[0] = hash;
            group->m_age = age;
            return uint4(group->m_x, group->m_y, m_quadSize.x, m_quadSize.y);
        } else {

        }
        return uint4(0,0,0,0);
    }

    ShadowCache::ShadowGroup* ShadowCache::allocGroup() {
        uint32_t index = m_pool.requestId();
        ShadowGroup* entry = &m_alloc[index];
        std::memset(entry, 0, sizeof(ShadowGroup));
        entry->m_parent = UINT16_MAX;
        entry->m_child = UINT16_MAX;
        entry->m_next = UINT16_MAX;
        entry->m_prev = UINT16_MAX;
        entry->m_idx = index;
        return entry;
    }

    void ShadowCache::updateAge(ShadowCache::ShadowGroup* current, uint32_t age) {
        for(uint16_t i = current->m_idx; i != UINT16_MAX; i = m_alloc[i].m_parent) {
            if(age > current->m_age ) {
               current->m_age = age;
            }
        }
    }

    void ShadowCache::freeChildren(ShadowCache::ShadowGroup* grp) {
        if(grp->m_child != UINT16_MAX) {
            for(uint32_t i = grp->m_child; i != UINT16_MAX && m_alloc[i].m_parent == grp->m_idx; i = m_alloc[i].m_next) {
                m_alloc[i].m_parent = UINT16_MAX; // we invalidate the parent
                freeGroup(&m_alloc[i]);
            }
        }
        std::memset(grp->m_hashes, 0, sizeof(grp->m_hashes));
        grp->m_occupyMask = 0;
        grp->m_flags &= ~(ShadowGroup::HasLeafs | ShadowGroup::HasGroups);
    }
    void ShadowCache::freeGroup(ShadowCache::ShadowGroup* grp) {
        for(auto& cut: m_cut) {
            if(cut.m_begin == grp->m_idx) {
                cut.m_begin = grp->m_next; // we assign the next if UINT16_MAX then that is accepted the layer is now empty
            }
        }
        if(grp->m_child != UINT16_MAX) {
            for(uint32_t i = grp->m_child; i != UINT16_MAX && m_alloc[i].m_parent == grp->m_idx; i = m_alloc[i].m_next) {
                m_alloc[i].m_parent = UINT16_MAX; // we invalidate the parent
                freeGroup(&m_alloc[i]);
            }
        }
        if(grp->m_parent != UINT16_MAX) {
            auto* parent = &m_alloc[grp->m_parent];
            parent->m_occupyMask &= ~(1 << grp->m_slot); // we clear the occupy slot
            if(parent->m_child == grp->m_idx) {
                // if the next child has a mismatched index we've exausted cuts
                if(grp->m_next != UINT16_MAX && m_alloc[grp->m_next].m_parent == parent->m_idx) {
                    parent->m_child = grp->m_next;
                }

            }
        }
        if(grp->m_prev != UINT16_MAX && grp->m_next != UINT16_MAX) {
            m_alloc[grp->m_prev].m_next = m_alloc[grp->m_next].m_idx;
            m_alloc[grp->m_next].m_prev = m_alloc[grp->m_prev].m_idx;
        } else if(grp->m_prev != UINT16_MAX) {
            m_alloc[grp->m_prev].m_next = UINT16_MAX;
        } else if(grp->m_next != UINT16_MAX) {
            m_alloc[grp->m_next].m_prev = UINT16_MAX;
        }
        m_pool.returnId(grp->m_idx);
    }
}
