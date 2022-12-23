#pragma once

#include "absl/types/span.h"
#include <absl/container/inlined_vector.h>
#include <graphics/Image.h>
#include <bgfx/bgfx.h>
#include <memory>

namespace hpl {
class RenderTarget {
public:
    RenderTarget(std::shared_ptr<Image> image);
    RenderTarget(absl::Span<std::shared_ptr<Image>> images);
    RenderTarget(RenderTarget&& target);
    RenderTarget();
    ~RenderTarget();

    void operator=(RenderTarget&& target);

    const bgfx::FrameBufferHandle GetHandle() const;
    const ImageDescriptor& GetDescriptor(size_t index) const;

private:
    absl::InlinedVector<std::shared_ptr<Image>, 7> m_image;
    bgfx::FrameBufferHandle m_buffer;
};

}