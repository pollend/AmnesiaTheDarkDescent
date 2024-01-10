#include "graphics/BindlessDescriptorPool.h"
#include "graphics/ForgeHandles.h"

namespace hpl {

    BindlessDescriptorPool::BindlessDescriptorPool(uint32_t ringsize, uint32_t poolSize):
        m_pool(poolSize) {
        m_ring.resize(ringsize);
        m_slot.resize(poolSize);
    }
    BindlessDescriptorPool::BindlessDescriptorPool():
        m_pool(0) {
        m_ring.resize(0);
        m_slot.resize(0);
    }
    void BindlessDescriptorPool::reset(DescriptorHandler handler) {
        m_actionHandler = handler;
        m_index = (m_index + 1) % m_ring.size();
        for(auto& ringEntry: m_ring[m_index]) {
            handler(ringEntry.m_action, ringEntry.slot, m_slot[ringEntry.slot].m_texture);
            ringEntry.m_count++;
            switch (ringEntry.m_action) {
            case Action::UpdateSlot:
                {
                    if (ringEntry.m_count < m_ring.size()) {
                        m_ring[(m_index + 1) % m_ring.size()].push_back(ringEntry);
                    }
                    break;
                }
            case Action::RemoveSlot:
                {
                    if (ringEntry.m_count <= m_ring.size()) {
                        m_ring[(m_index + 1) % m_ring.size()].push_back(ringEntry);
                    } else {
                        m_pool.returnId(ringEntry.slot);
                        m_slot[ringEntry.slot].m_texture = SharedTexture();
                    }
                }
            }
        }
        m_ring[m_index].clear();
    }
     uint32_t BindlessDescriptorPool::request(SharedTexture& texture) {
        for(size_t i = 0; i < m_slot.size(); i++) {
            if(m_slot[i].m_texture.m_handle == texture.m_handle) {
                m_slot[i].m_count++;
                return i;
            }
        }

        uint32_t slot = m_pool.requestId();
        ASSERT(slot != IndexPool::InvalidHandle);
        m_slot[slot].m_texture = texture;
        m_slot[slot].m_count++;
        m_actionHandler(Action::UpdateSlot, slot, m_slot[slot].m_texture);
        if(m_ring.size() > 1) {
            m_ring[(m_index + 1) % m_ring.size()].push_back(RingEntry {Action::UpdateSlot, slot, 1});
        }
        return slot;
    }
    void BindlessDescriptorPool::dispose(uint32_t slot) {
        ASSERT(slot < m_slot.size());
        ASSERT(m_slot[slot].m_count > 0);
        m_slot[slot].m_count--;
        if(m_slot[slot].m_count == 0) {
            m_ring[(m_index + 1) % m_ring.size()].push_back(RingEntry {Action::RemoveSlot, slot, 0});
        }
    }
} // namespace hpl
