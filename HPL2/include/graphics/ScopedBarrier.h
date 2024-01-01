#pragma once

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include <span>

namespace hpl {
    class RenderTargetScopedBarrier;
    struct RenderTargetTransition {
        RenderTargetScopedBarrier* m_resource;
        ResourceState m_newState;
    };

    namespace ScopedBarrier {
        void End(
            Cmd* cmd,
            std::span<RenderTargetScopedBarrier*>
        );

        void Transition(
            Cmd* cmd,
            std::span<RenderTargetTransition>
        );
    };

    class RenderTargetScopedBarrier {
    public:
        RenderTargetScopedBarrier();
        RenderTargetScopedBarrier(RenderTarget* target, ResourceState start, ResourceState end);
        RenderTargetScopedBarrier(RenderTargetScopedBarrier&) = delete;
        RenderTargetScopedBarrier(RenderTargetScopedBarrier&&);

        void Set(RenderTarget* target, ResourceState start, ResourceState end);
        void operator=(RenderTargetBarrier&) = delete;
        void operator=(RenderTargetScopedBarrier&&);

        ~RenderTargetScopedBarrier() {
            // scope hasn't been closed
            ASSERT(m_target == nullptr);
        }

        inline bool IsValid() {
            return m_target != nullptr;
        }

        inline RenderTarget* Target() {
            return m_target;
        }

    private:
        ResourceState m_startState;
        ResourceState m_currentState;
        ResourceState m_endState;
        RenderTarget* m_target = nullptr;

        friend void ScopedBarrier::End(Cmd*, std::span<RenderTargetScopedBarrier*>);
        friend void ScopedBarrier::Transition(Cmd*, std::span<RenderTargetTransition>);

    };


}
