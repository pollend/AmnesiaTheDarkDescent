#pragma once

#include <bgfx/bgfx.h>

class RenderTarget {
    bgfx::TextureHandle _texture;
    bgfx::FrameBufferHandle _buffer;
};
