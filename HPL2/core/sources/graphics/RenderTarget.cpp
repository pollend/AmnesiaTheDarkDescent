#include "bgfx/bgfx.h"
#include <graphics/RenderTarget.h>

namespace hpl
{
    RenderTarget::RenderTarget(std::shared_ptr<Image> image)
        : _image(image)
    {
        bgfx::TextureHandle handle = image->GetHandle();
        _buffer = bgfx::createFrameBuffer(1, &handle, true);
    }

    const ImageDescriptor& RenderTarget::GetDescriptor() const
    {
        return _image->GetDescriptor();
    }

    bgfx::FrameBufferHandle RenderTarget::GetHandle()
    {
        return _buffer;
    }
} // namespace hpl
