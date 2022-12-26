#include "absl/types/span.h"
#include "math/MathTypes.h"
#include <cinttypes>
#include <graphics/RenderViewport.h>

namespace hpl {

    RenderViewport::RenderViewport(RenderTarget&& renderTarget, cVector2l position)
        : m_renderTarget(std::move(renderTarget))
        , m_position(position)
    {
    }

    RenderViewport::RenderViewport(RenderViewport&& viewport)
        : m_renderTarget(std::move(viewport.m_renderTarget))
        , m_position(viewport.m_position)
    {
    }

    RenderViewport::RenderViewport()
        : m_renderTarget()
        , m_position(0, 0)
    {
    }

    RenderViewport::~RenderViewport() {
    }

    void RenderViewport::operator=(RenderViewport&& viewport) {
        m_renderTarget = std::move(viewport.m_renderTarget);
        m_position = viewport.m_position;
    }

    RenderTarget& RenderViewport::GetRenderTarget() {
        return m_renderTarget;
    }

    const cVector2l RenderViewport::GetPosition() const {
        return m_position;
    }

    const cVector2l RenderViewport::GetSize() const {
        auto image = m_renderTarget.GetImage();
        if(auto img = image.lock()) {
            auto& desc = img->GetDescriptor();
            return cVector2l(desc.m_width, desc.m_height);
        }
        return cVector2l(0, 0);
        
    }

    void RenderViewport::setPosition(const cVector2l& position) {
        m_position = position;
    }

    void RenderViewport::setSize(const cVector2l& size) {
        for(auto& images: m_renderTarget.GetImages()) {
            auto desc = images->GetDescriptor();
            desc.m_width = size.x;
            desc.m_height = size.y;
            images->Invalidate();
            images->Initialize(desc);
        }
        absl::InlinedVector<std::shared_ptr<Image>, 7> result;
        auto targets = m_renderTarget.GetImages();
        result.insert(result.begin(), targets.begin(), targets.end());
        m_renderTarget.Invalidate();
        m_renderTarget.Initialize(absl::MakeSpan(result));
    }

    const bool RenderViewport::IsValid() const {
        return m_renderTarget.IsValid();
    }
}