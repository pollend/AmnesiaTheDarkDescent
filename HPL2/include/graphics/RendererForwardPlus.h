
#pragma once
#include "engine/RTTI.h"

#include "scene/Viewport.h"
#include "scene/World.h"
#include "windowing/NativeWindow.h"

#include <graphics/ForgeHandles.h>
#include <graphics/ForgeRenderer.h>
#include <graphics/Image.h>
#include <graphics/Material.h>
#include <graphics/PassHBAOPlus.h>
#include <graphics/RenderList.h>
#include <graphics/RenderTarget.h>
#include <graphics/Renderable.h>
#include <graphics/Renderer.h>
#include <math/MathTypes.h>

#include <Common_3/Graphics/Interfaces/IGraphics.h>
#include <Common_3/Utilities/RingBuffer.h>
#include <FixPreprocessor.h>

namespace hpl {
    class RendererForwardPlus : public iRenderer {
        HPL_RTTI_IMPL_CLASS(iRenderer, cRendererDeferred, "{A3E5E5A1-1F9C-4F5C-9B9B-5B9B9B5B9B9B}")
    public:
    };
} // namespace hpl
