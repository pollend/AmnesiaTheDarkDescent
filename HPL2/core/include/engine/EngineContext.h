#pragma once

#include "graphics/RenderTarget.h"
#include <graphics/GraphicsContext.h>
#include <memory>

namespace hpl::context {
    void Init();

    hpl::GraphicsContext& GraphicsContext();

    // post output before GUI
    void SetPostRenderOutput(std::shared_ptr<Image> image);
    RenderTarget& postRenderOutput();
}
