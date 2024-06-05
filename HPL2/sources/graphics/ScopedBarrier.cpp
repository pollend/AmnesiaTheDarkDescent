#include "graphics/ScopedBarrier.h"
#include "Common_3/Graphics/Interfaces/IGraphics.h"

#include <vector>

namespace hpl {
    RenderTargetScopedBarrier::RenderTargetScopedBarrier(): m_target(nullptr) {
    }

    RenderTargetScopedBarrier::RenderTargetScopedBarrier(RenderTarget* target, ResourceState start, ResourceState end)
        : m_target(target)
        , m_startState(start)
        , m_endState(end)
        , m_currentState(start) {
    }
    void RenderTargetScopedBarrier::Set(RenderTarget* target, ResourceState start, ResourceState end) {
       ASSERT(m_target == nullptr); // the scope can't be set twice
       m_target = target;
       m_startState = start;
       m_currentState = start;
       m_endState = end;
    }
    RenderTargetScopedBarrier::RenderTargetScopedBarrier(RenderTargetScopedBarrier&& barrier)
        : m_target(barrier.m_target)
        , m_startState(barrier.m_startState)
        , m_currentState(barrier.m_currentState) {
        barrier.m_target = nullptr;
    }
    void RenderTargetScopedBarrier::operator=(RenderTargetScopedBarrier&& barrier) {
       m_target = barrier.m_target;
       m_startState = barrier.m_startState;
       m_currentState = barrier.m_currentState;
       m_endState = barrier.m_endState;
      barrier.m_target = nullptr;
    }
    namespace ScopedBarrier {
        void End(Cmd* cmd, std::span<RenderTargetScopedBarrier*> barriers) {
            std::vector<RenderTargetBarrier> rtBarriers;
            for (auto& barrier : barriers) {
                if (barrier->m_currentState != barrier->m_endState && barrier->IsValid()) {
                    rtBarriers.push_back(RenderTargetBarrier{ barrier->m_target, barrier->m_currentState, barrier->m_endState });
                    barrier->m_target = nullptr;
                }
            }
            cmdResourceBarrier(cmd, 0, nullptr, 0, nullptr, rtBarriers.size(), rtBarriers.data());
        }

        void Transition(
            Cmd* cmd,
            std::span<RenderTargetTransition> transitions
        ) {
            std::vector<RenderTargetBarrier> rtBarriers;

            for(auto& tran: transitions) {
                if(tran.m_newState != tran.m_resource->m_currentState && tran.m_resource->m_target != nullptr) {
                    rtBarriers.push_back(RenderTargetBarrier{ tran.m_resource->m_target, tran.m_resource->m_currentState, tran.m_newState});
                    tran.m_resource->m_currentState = tran.m_newState;
                }
            }
            cmdResourceBarrier(cmd, 0, nullptr, 0, nullptr, rtBarriers.size(), rtBarriers.data());
        }
    } // namespace ScopedBarrier

} // namespace hpl
