#pragma once

#include "math/MathTypes.h"
#include <graphics/RenderTarget.h>
#include <memory>

namespace hpl
{
    //! Container for a RenderTarget and a position and size
    class RenderViewport
    {
    public:
        RenderViewport(std::shared_ptr<RenderTarget> renderTarget, cVector2l position, cVector2l size = cVector2l(-1, -1));
        RenderViewport();
        ~RenderViewport();

        // void operator=(const RenderViewport& viewport) = delete;
        // void operator=(RenderViewport&& viewport);

        std::shared_ptr<RenderTarget>& GetRenderTarget();
        const cVector2l GetPosition() const;
        const cVector2l GetSize() const;

        void setRenderTarget(std::shared_ptr<RenderTarget> target) {
            m_renderTarget = target;
        }

        void setPosition(const cVector2l& position);
        void setSize(const cVector2l& size);
        
        const bool IsValid() const;

    private:
        std::shared_ptr<RenderTarget> m_renderTarget = nullptr;
        cVector2l m_position = cVector2l(0, 0);
        cVector2l m_size = cVector2l(-1, -1);
    };
} // namespace hpl