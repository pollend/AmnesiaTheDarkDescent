#pragma once

#include "graphics/ForgeHandles.h"
#include "graphics/Renderer.h"
#include <cstdint>

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "FixPreprocessor.h"

namespace hpl {
    template<uint32_t COUNT>
    class CommandBufferPool {
    public:
        struct PoolEntry {
        public:
            SharedCmdPool m_pool;
            SharedCmd m_cmd;
            SharedFence m_fence;
        };
        CommandBufferPool() {}
        CommandBufferPool(CommandBufferPool<COUNT>&&) = default;
        CommandBufferPool(const CommandBufferPool<COUNT>&) = delete;

        void operator=(const CommandBufferPool<COUNT>&) = delete;
        CommandBufferPool<COUNT>& operator=(CommandBufferPool<COUNT>&&) = default;

        CommandBufferPool(Renderer* renderer, Queue* queue): m_renderer(renderer){
            for(auto& entry: m_pool) {
                entry.m_pool.Load(renderer, [&](CmdPool** pool) {
                    CmdPoolDesc cmdPoolDesc = {};
                    cmdPoolDesc.pQueue = queue;
                    addCmdPool(renderer, &cmdPoolDesc, pool);
                    return true;
                });
                entry.m_cmd.Load(renderer, [&](Cmd** cmd) {
                    CmdDesc cmdDesc = {};
                    cmdDesc.pPool = entry.m_pool.m_handle;
                    addCmd(renderer, &cmdDesc, cmd);
                    return true;
                });
                entry.m_fence.Load(renderer, [&](Fence** fence) {
                    addFence(renderer, fence);
                    return true;
                });
            }
        }

        void waitAll() {
            for(auto& entry: m_pool) {
                FenceStatus status{};
                getFenceStatus(m_renderer, entry.m_fence.m_handle, &status);
                if(status == FENCE_STATUS_INCOMPLETE) {
                    waitForFences(m_renderer, 1, &entry.m_handle);
                }
            }
        }
        PoolEntry* findAvaliableEntry() {
           for(auto& entry: m_pool) {
                FenceStatus status{};
                getFenceStatus(m_renderer, entry.m_fence.m_handle, &status);
                if(status == FENCE_STATUS_COMPLETE) {
                    return &entry;
                }
           }
            return nullptr;
        }
    private:
        Renderer* m_renderer;
        std::array<PoolEntry, COUNT> m_pool;
    };
}
