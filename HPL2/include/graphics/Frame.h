#pragma once

#include <graphics/ForgeRenderer.h>

namespace hpl {
    /**
     * A variable that is only peristent for the frame
     */
    template<typename T>
    class TransientFrameVariable final {
    public:
        //static std::function<void(T&)> defaultOnReset = ;

        inline T& Fetch(
            ForgeRenderer::Frame& frame, std::function<void(T&)> onReset = [](T& value) {
                value = T();
            }) {
            if (frame.FrameCount() != m_frameCount) {
                m_frameCount = frame.FrameCount();
                onReset(m_value);
            }
            return m_value;
        }

    private:
        uint32_t m_frameCount;
        T m_value;
    };

    class ResetFrameHandler final {
    public:
        bool Check(ForgeRenderer::Frame& frame) {
            if (frame.FrameCount() != m_frameCount) {
                m_frameCount = frame.FrameCount();
                return true;
            }
            return false;
        }

    private:
        uint32_t m_frameCount = 0;
    };

    //class Frame {
    //public:
    //    uint32_t m_currentFrame = 0;
    //    uint32_t m_frameIndex = 0;
    //    GpuCmdRingElement m_cmdRingElement;
    //    bool m_isFinished = true;
    //    // FrameState m_state = FrameState::Unitialize;
    //
    //    ForgeRenderer* m_renderer = nullptr;
    //    SwapChain* m_swapChain = nullptr;
    //    Cmd* m_cmd = nullptr;
    //    CmdPool* m_cmdPool = nullptr;
    //    Fence* m_renderCompleteFence = nullptr;
    //    Semaphore* m_renderCompleteSemaphore = nullptr;
    //    CommandResourcePool* m_resourcePool = nullptr;
    //    RenderTarget* m_finalRenderTarget = nullptr;
    //
    //    template<typename T>
    //    inline void pushResource(const T& ele) {
    //        m_resourcePool->Push(ele);
    //    }
    //
    //    inline uint32_t FrameCount() {
    //        return m_currentFrame;
    //    }
    //
    //    inline uint32_t index() {
    //        return m_frameIndex;
    //    }
    //
    //    inline Cmd*& cmd() {
    //        return m_cmdRingElement.pCmds[0];
    //    }
    //
    //    inline RenderTarget* finalTarget() {
    //        return m_finalRenderTarget;
    //    }
    //
    //    // inline RenderTarget* swapChainTarget() {
    //    //     return m_swapChainTarget;
    //    // }
    //
    //    inline GpuCmdRingElement& RingElement() {
    //        return m_cmdRingElement;
    //    }
    //
    //    friend class ForgeRenderer;
    //};
} // namespace hpl
