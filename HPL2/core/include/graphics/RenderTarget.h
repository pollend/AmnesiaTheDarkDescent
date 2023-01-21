#pragma once

#include "math/MathTypes.h"
#include <absl/container/inlined_vector.h>
#include <absl/types/span.h>
#include <bgfx/bgfx.h>
#include <cstdint>
#include <graphics/Image.h>
#include <memory>

namespace hpl
{
    class RenderTarget
    {
    public:
        static const RenderTarget EmptyRenderTarget;

        RenderTarget(std::shared_ptr<Image> image);
        RenderTarget(absl::Span<std::shared_ptr<Image>> images);

        RenderTarget(RenderTarget&& target);
        RenderTarget(const RenderTarget& target) = delete;
        RenderTarget();
        ~RenderTarget();

        void Initialize(absl::Span<std::shared_ptr<Image>> descriptor);
        void Invalidate();
        const bool IsValid() const;

        void operator=(const RenderTarget& target) = delete;
        void operator=(RenderTarget&& target);

        const bgfx::FrameBufferHandle GetHandle() const;
        absl::Span<std::shared_ptr<Image>> GetImages();
        std::shared_ptr<Image> GetImage(size_t index = 0);

        const absl::Span<const std::shared_ptr<Image>> GetImages() const;
        const std::shared_ptr<Image> GetImage(size_t index = 0) const;

    private:
        absl::InlinedVector<std::shared_ptr<Image>, 7> m_images;
        bgfx::FrameBufferHandle m_buffer = BGFX_INVALID_HANDLE;
    };

} // namespace hpl