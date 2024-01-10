#pragma once

#include "graphics/ForgeHandles.h"
#include "graphics/IndexPool.h"

namespace hpl {
    class BindlessDescriptorPool final {
    public:
        enum Action {
            UpdateSlot,
            RemoveSlot
        };
        using DescriptorHandler = std::function<void(Action action, uint32_t slot, SharedTexture& texture)>;
        BindlessDescriptorPool();
        explicit BindlessDescriptorPool(uint32_t ringsize, uint32_t poolSize);
        void reset(DescriptorHandler handler);
        uint32_t request(SharedTexture& texture);
        void dispose(uint32_t slot);
    private:

        struct SlotInfo {
            SharedTexture m_texture;
            uint8_t m_count = 0;
        };

        struct RingEntry {
            Action m_action;
            uint32_t slot;
            uint8_t m_count;
        };
        uint32_t m_index = 0;
        IndexPool m_pool;
        folly::small_vector<std::vector<RingEntry>> m_ring;
        std::vector<SlotInfo> m_slot;
        DescriptorHandler m_actionHandler;
    };
} // namespace hpl
