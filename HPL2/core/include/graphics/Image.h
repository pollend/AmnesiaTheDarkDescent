#pragma once

#include <graphics/GraphicsTypes.h>
#include <bgfx/bgfx.h>

namespace hpl
{
     struct ImageDescriptor {
        ImageDescriptor();
        ImageDescriptor(const ImageDescriptor& desc);

        uint16_t m_width;
        uint16_t m_height;
        uint16_t m_arraySize;
        bool m_hasMipMaps;

        static ImageDescriptor CreateTexture2D(uint16_t width, uint16_t height, bool hasMipMaps, bgfx::TextureFormat::Enum format);
        
        bgfx::TextureFormat::Enum format;

    };

    class Image
    {
    public:
        Image();
        ~Image();

        // bool isValid() const { return true;}
        void Initialize(const ImageDescriptor& descriptor,const bgfx::Memory* mem = nullptr);
        void Invalidate();

        bgfx::TextureHandle GetHandle() const;
        const ImageDescriptor& GetDescriptor() const;

        static bgfx::TextureFormat::Enum FromHPLTextureFormat(ePixelFormat format);
    private:
        bgfx::TextureHandle _handle = BGFX_INVALID_HANDLE;
        ImageDescriptor _descriptor = ImageDescriptor();
    };
} // namespace hpl