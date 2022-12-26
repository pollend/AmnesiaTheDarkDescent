#pragma once

#include <graphics/RenderTarget.h>

namespace hpl
{
    //! Container for a RenderTarget and a position and size
    class RenderViewport
    {
    public:
        RenderViewport(RenderTarget&& renderTarget, cVector2l position);
        RenderViewport(RenderViewport&& viewport);
        RenderViewport(const RenderViewport& viewport) = delete;
        RenderViewport();
        ~RenderViewport();

        void operator=(const RenderViewport& viewport) = delete;
        void operator=(RenderViewport&& viewport);

        RenderTarget& GetRenderTarget();
        const cVector2l GetPosition() const;
        const cVector2l GetSize() const;

        void setPosition(const cVector2l& position);
        void setSize(const cVector2l& size);
        
        const bool IsValid() const;

    private:
        RenderTarget m_renderTarget;
        cVector2l m_position;
    };
} // namespace hpl