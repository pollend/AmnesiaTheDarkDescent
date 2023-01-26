#pragma once


namespace hpl {
    class Image;
    class cTextureManager;
    class AnimatedImage;
    class iResourceBase;

    struct ImageResourceWrapper {
    public:
        ImageResourceWrapper();
        ImageResourceWrapper(cTextureManager* m_textureManager, hpl::iResourceBase* resource);
        ImageResourceWrapper(const ImageResourceWrapper& other) = delete;
        ImageResourceWrapper(ImageResourceWrapper&& other);
        ~ImageResourceWrapper();

        void SetAutoDestroyResource(bool autoDestroyTextures);

        void operator=(const ImageResourceWrapper&& other) = delete;
        void operator=(ImageResourceWrapper&& other);

        Image* GetImage() const;

    private:
        hpl::iResourceBase* m_imageResource = nullptr;
        cTextureManager* m_textureManager = nullptr;
        bool m_autoDestroyResource = true;
    };
} // namespace hpl