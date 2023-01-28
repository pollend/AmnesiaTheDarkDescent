#include "bgfx/bgfx.h"
#include <bx/debug.h>
#include <cstddef>
#include <graphics/AnimatedImage.h>

namespace hpl {

    AnimatedImage::AnimatedImage()
        : iResourceBase("", _W(""), 0) {
    }

    AnimatedImage::AnimatedImage(const tString& asName, const tWString& asFullPath)
        : iResourceBase(asName, asFullPath, 0) {
    }
    
    AnimatedImage::~AnimatedImage() {

    }

    Image* AnimatedImage::GetImage() const {
        size_t lFrame = static_cast<size_t>(m_timeCount);
        BX_ASSERT(lFrame < m_images.size(), "Frame index out of range");

        auto* image = m_images[lFrame].get();
        BX_ASSERT(image, "Image is null");
        return image;
    }

    bool AnimatedImage::Reload() {
        return false;
    }
    void AnimatedImage::Unload() {
        
    }
    void AnimatedImage::Destroy() {
        
    }
    void AnimatedImage::SetFrameTime(float frameTime) {
        m_frameTime = frameTime;
    }

    float AnimatedImage::GetFrameTime() {
        return m_frameTime;
    }
    
    eTextureAnimMode AnimatedImage::GetAnimMode() {
        return m_animMode;
    }
    
    void AnimatedImage::SetAnimMode(eTextureAnimMode aMode) {
        m_animMode = aMode;
    }
    
    void AnimatedImage::Update(float afTimeStep) {
        if (m_images.size() > 1) {
            float fMax = (float)(m_images.size());
            m_timeCount += afTimeStep * (1.0f / m_frameTime) * m_timeDir;

            if (m_timeDir > 0) {
                if (m_timeCount >= fMax) {
                    if (m_animMode == eTextureAnimMode_Loop) {
                        m_timeCount = 0;
                    } else {
                        m_timeCount = fMax - 1.0f;
                        m_timeDir = -1.0f;
                    }
                }
            } else {
                if (m_timeCount < 0) {
                    m_timeCount = 1;
                    m_timeDir = 1.0f;
                }
            }
        }
    }

    void AnimatedImage::Initialize(std::span<std::unique_ptr<hpl::Image>> images) {
        m_images.clear();
        for (auto& image : images) {
            m_images.push_back(std::move(image));
        }
    }

    bgfx::TextureHandle AnimatedImage::GetHandle() const {
        auto* image = GetImage();
        BX_ASSERT(bgfx::isValid(image->GetHandle()), "Image handle is invalid");
        return image->GetHandle();
    }

} // namespace hpl