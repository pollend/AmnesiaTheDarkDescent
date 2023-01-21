#include "graphics/GraphicsContext.h"
#include <engine/EngineContext.h>
#include <gui/GuiSet.h>
#include <memory>

namespace hpl::context {

    struct EngineContext {
        hpl::GraphicsContext graphicsContext;

        std::unique_ptr<RenderTarget> m_outputTarget;
    };
    static EngineContext s_ctx;

    void Init() {
        s_ctx.graphicsContext.Init();
		cGuiSet::Init();
    }


    void SetPostRenderOutput(std::shared_ptr<Image> image) {
        s_ctx.m_outputTarget = std::make_unique<RenderTarget>(image);
    }

    hpl::GraphicsContext& GraphicsContext() {
        return s_ctx.graphicsContext;
    }

    RenderTarget& postRenderOutput() {
        return *s_ctx.m_outputTarget;
    }
}