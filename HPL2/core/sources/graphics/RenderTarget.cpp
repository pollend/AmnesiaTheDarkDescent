#include "bgfx/bgfx.h"
#include <graphics/RenderTarget.h>

namespace hpl
{
    RenderTarget::RenderTarget(std::shared_ptr<Image> image)
        : m_image({image})
    {
        bgfx::TextureHandle handle = image->GetHandle();
        m_buffer = bgfx::createFrameBuffer(1, &handle, false);
    }

    RenderTarget::RenderTarget(absl::Span<std::shared_ptr<Image>> images) {
        m_image.insert(m_image.end(), images.begin(), images.end());
        absl::InlinedVector<bgfx::TextureHandle, 7> handles = {}; 
        for (auto& image : m_image) {
            handles.push_back(image->GetHandle());
        }
        m_buffer = bgfx::createFrameBuffer(handles.size(), handles.data(), false);
    }

    RenderTarget::RenderTarget()
        : m_image({})
    {
    }

    RenderTarget::RenderTarget(RenderTarget&& target) {
        m_image = std::move(target.m_image);
        m_buffer = target.m_buffer;
        target.m_buffer = BGFX_INVALID_HANDLE;
    }

    void RenderTarget::operator=(RenderTarget&& target) {
        m_image = std::move(target.m_image);
        m_buffer = target.m_buffer;
        target.m_buffer = BGFX_INVALID_HANDLE;
    }

    RenderTarget::~RenderTarget() {
        if (bgfx::isValid(m_buffer)) {
            bgfx::destroy(m_buffer);
        }
    }

    const ImageDescriptor& RenderTarget::GetDescriptor(size_t index) const
    {
        return m_image[index]->GetDescriptor();
    }

    const bgfx::FrameBufferHandle RenderTarget::GetHandle() const
    {
        return m_buffer;
    }
} // namespace hpl
