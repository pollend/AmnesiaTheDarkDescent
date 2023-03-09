#include <graphics/ImageResourceWrapper.h>

#include <bx/debug.h>
#include <resources/TextureManager.h>
#include <graphics/Image.h>
#include <engine/RTTI.h>
#include <graphics/AnimatedImage.h>

namespace hpl {

    ImageResourceWrapper::ImageResourceWrapper()
        : m_imageResource(nullptr)
        , m_textureManager(nullptr) {
    }
    ImageResourceWrapper::ImageResourceWrapper(cTextureManager* m_textureManager, hpl::iResourceBase* resource)
        : m_imageResource(resource)
        , m_textureManager(m_textureManager) {
        BX_ASSERT(m_imageResource, "ImageResourceWrapper: Image resource is null");
        BX_ASSERT(m_textureManager, "ImageResourceWrapper: Texture manager is null");
    }
    ImageResourceWrapper::ImageResourceWrapper(ImageResourceWrapper&& other) {
        if (m_imageResource && m_autoDestroyResource) {
            m_textureManager->Destroy(m_imageResource);
        }
        m_imageResource = other.m_imageResource;
        m_textureManager = other.m_textureManager;
        m_autoDestroyResource = other.m_autoDestroyResource;

        other.m_imageResource = nullptr;
        other.m_textureManager = nullptr;
    }
    
    ImageResourceWrapper::~ImageResourceWrapper() {
        if (m_imageResource && m_autoDestroyResource) {
            m_textureManager->Destroy(m_imageResource);
        }
    }

    void ImageResourceWrapper::operator=(ImageResourceWrapper&& other) {
        if (m_imageResource && m_autoDestroyResource) {
            m_textureManager->Destroy(m_imageResource);
        }
        m_imageResource = other.m_imageResource;
        m_textureManager = other.m_textureManager;
        m_autoDestroyResource = other.m_autoDestroyResource;

        other.m_imageResource = nullptr;
        other.m_textureManager = nullptr;
    }

    void ImageResourceWrapper::SetAutoDestroyResource(bool autoDestroyResource) {
        m_autoDestroyResource = autoDestroyResource;
    }

    Image* ImageResourceWrapper::GetImage() const {
        if (m_imageResource == nullptr) {
            return nullptr;
        }

        if (TypeInfo<Image>().IsType(*m_imageResource)) {
            return static_cast<Image*>(m_imageResource);
        } else if (TypeInfo<AnimatedImage>().IsType(*m_imageResource)) {
            return static_cast<AnimatedImage*>(m_imageResource)->GetImage();
        }
        return nullptr;
    }
} // namespace hpl