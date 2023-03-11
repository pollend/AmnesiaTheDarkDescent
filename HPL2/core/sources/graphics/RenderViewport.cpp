
#include "graphics/RenderTarget.h"
#include "math/MathTypes.h"
#include <cinttypes>
#include <graphics/RenderViewport.h>
#include <bx/debug.h>

namespace hpl {

    RenderViewport::RenderViewport(std::shared_ptr<RenderTarget> renderTarget, cVector2l position, cVector2l size)
        : m_renderTarget(renderTarget)
        , m_position(position)
        , m_size(size)
    {
    }

    RenderViewport::RenderViewport()
        : m_renderTarget()
        , m_position(0, 0)
    {
    }

    RenderViewport::~RenderViewport() {
    }

    RenderTarget& RenderViewport::GetRenderTarget() {
        static RenderTarget emptyTarget = RenderTarget();
        if(m_renderTarget) {
            return emptyTarget;
        }
        return *m_renderTarget;
    }

    const cVector2l RenderViewport::GetPosition() const {
        return m_position;
    }

    const cVector2l RenderViewport::GetSize() const {
        return m_size;
    }

    void RenderViewport::setPosition(const cVector2l& position) {
        m_position = position;
    }

    void RenderViewport::setSize(const cVector2l& size) {
        m_size = size;
    }

    const bool RenderViewport::IsValid() const {
        return m_renderTarget->IsValid();
    }
}