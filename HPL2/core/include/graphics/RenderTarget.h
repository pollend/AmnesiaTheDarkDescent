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