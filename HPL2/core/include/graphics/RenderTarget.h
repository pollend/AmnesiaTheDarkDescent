#pragma once

#include "graphics/Image.h"
#include <bgfx/bgfx.h>
#include <memory>

namespace hpl {
class RenderTarget {
public:
    RenderTarget(std::shared_ptr<Image> image);

    bgfx::FrameBufferHandle GetHandle();
    const ImageDescriptor& GetDescriptor() const;

private:
    std::shared_ptr<Image> _image;
    bgfx::FrameBufferHandle _buffer;
};

}