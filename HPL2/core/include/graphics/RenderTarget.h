#pragma once

#include "math/MathTypes.h"
#include <absl/container/inlined_vector.h>
#include <bgfx/bgfx.h>
#include <cstdint>
#include <graphics/Image.h>
#include <memory>
#include <span>

namespace hpl
{
    class RenderTarget
    {
    public:
        static const RenderTarget EmptyRenderTarget;

        RenderTarget(std::shared_ptr<Image> image);
        RenderTarget(std::span<std::shared_ptr<Image>> images);

        RenderTarget(RenderTarget&& target);
        RenderTarget(const RenderTarget& target) = delete;
        RenderTarget();
        ~RenderTarget();

        void Initialize(std::span<std::shared_ptr<Image>> descriptor);
        void Invalidate();
        const bool IsValid() const;

        void operator=(const RenderTarget& target) = delete;
        void operator=(RenderTarget&& target);

        const bgfx::FrameBufferHandle GetHandle() const;
        std::span<std::shared_ptr<Image>> GetImages();
        std::shared_ptr<Image> GetImage(size_t index = 0);

        const std::span<const std::shared_ptr<Image>> GetImages() const;
        const std::shared_ptr<Image> GetImage(size_t index = 0) const;

    private:
        absl::InlinedVector<std::shared_ptr<Image>, 7> m_images;
        bgfx::FrameBufferHandle m_buffer = BGFX_INVALID_HANDLE;
    };

} // namespace hpl