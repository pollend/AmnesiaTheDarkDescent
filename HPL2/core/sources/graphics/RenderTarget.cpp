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

    RenderTarget::RenderTarget()
        : _image(nullptr)
    {
    }

    RenderTarget::RenderTarget(RenderTarget&& target) {
        _image = std::move(target._image);
        _buffer = target._buffer;
        target._buffer = BGFX_INVALID_HANDLE;
    }

    void RenderTarget::operator=(RenderTarget&& target) {
        _image = std::move(target._image);
        _buffer = target._buffer;
        target._buffer = BGFX_INVALID_HANDLE;
    }

    RenderTarget::~RenderTarget() {
        if (bgfx::isValid(_buffer)) {
            bgfx::destroy(_buffer);
        }
    }

    const ImageDescriptor& RenderTarget::GetDescriptor() const
    {
        return _image->GetDescriptor();
    }

    const bgfx::FrameBufferHandle RenderTarget::GetHandle() const
    {
        return _buffer;
    }
} // namespace hpl
