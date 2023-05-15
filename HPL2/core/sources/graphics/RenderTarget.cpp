/**
* Copyright 2023 Michael Pollind
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* 
*     http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "bgfx/bgfx.h"
#include "graphics/Image.h"
#include <array>
#include <graphics/RenderTarget.h>
#include <memory>
#include <bx/debug.h>
#include <span>

namespace hpl
{
    const LegacyRenderTarget LegacyRenderTarget::EmptyRenderTarget = LegacyRenderTarget();

    LegacyRenderTarget::LegacyRenderTarget(std::shared_ptr<Image> image)
    {
        std::array<std::shared_ptr<Image>, 1> images = { image };
        Initialize(std::span(images));
    }

    LegacyRenderTarget::LegacyRenderTarget(std::span<std::shared_ptr<Image>> images)
    {
        Initialize(images);
    }

    LegacyRenderTarget::LegacyRenderTarget()
        : m_images({})
    {
    }

    LegacyRenderTarget::LegacyRenderTarget(LegacyRenderTarget&& target)
    {
        Invalidate();
        m_images = std::move(target.m_images);
        m_buffer = target.m_buffer;
        target.m_buffer = BGFX_INVALID_HANDLE;
    }

    void LegacyRenderTarget::operator=(LegacyRenderTarget&& target)
    {
        Invalidate();
        m_images = std::move(target.m_images);
        m_buffer = target.m_buffer;
        target.m_buffer = BGFX_INVALID_HANDLE;
    }

    LegacyRenderTarget::~LegacyRenderTarget()
    {
        if (bgfx::isValid(m_buffer))
        {
            bgfx::destroy(m_buffer);
        }
    }

    void LegacyRenderTarget::Initialize(std::span<std::shared_ptr<Image>> images) {
        BX_ASSERT(!bgfx::isValid(m_buffer), "RenderTarget already initialized");
        
        absl::InlinedVector<bgfx::TextureHandle, 7> handles = {};
        absl::InlinedVector<std::shared_ptr<Image>, 7> updateImages = {};
        updateImages.insert(updateImages.end(), images.begin(), images.end());
        for (auto& image : updateImages)
        {
            // auto& descriptor = image->GetDescriptor();
            // BX_ASSERT(descriptor.m_configuration.m_rt != RTType::None, "Image is not a render target");
            handles.push_back(image->GetHandle());
        }
        m_buffer = bgfx::createFrameBuffer(handles.size(), handles.data(), false);
        m_images = std::move(updateImages);
    }

    void LegacyRenderTarget::Invalidate() {
        if(bgfx::isValid(m_buffer)) {
            bgfx::destroy(m_buffer);
        }
        m_images = {};
        m_buffer = BGFX_INVALID_HANDLE;
    }

    const bool LegacyRenderTarget::IsValid() const
    {
        return bgfx::isValid(m_buffer);
    }

    std::shared_ptr<Image> LegacyRenderTarget::GetImage(size_t index){
        return m_images[index];
    }

    std::span<std::shared_ptr<Image>> LegacyRenderTarget::GetImages()
    {
        return std::span(m_images.begin(), m_images.end());
    }
    
    const std::shared_ptr<Image> LegacyRenderTarget::GetImage(size_t index) const {
        return m_images[index];
    }

    const std::span<const std::shared_ptr<Image>> LegacyRenderTarget::GetImages() const 
    {
        return std::span(m_images.begin(), m_images.end());
    }

    const bgfx::FrameBufferHandle LegacyRenderTarget::GetHandle() const
    {
        return m_buffer;
    }
} // namespace hpl
