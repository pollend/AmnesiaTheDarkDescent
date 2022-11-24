#pragma once

#include "graphics/Bitmap.h"
#include "graphics/GraphicsTypes.h"
#include <bgfx/bgfx.h>
#include <cstddef>
#include <cstdint>

namespace hpl
{
     struct ImageDescriptor {
        ImageDescriptor();
        ImageDescriptor(const ImageDescriptor& desc);

        uint16_t width;
        uint16_t height;

        bgfx::TextureFormat::Enum format;

        // static uint32_t GetNumberChannels(bgfx::TextureFormat::Enum format);
    };

    class Image
    {
    public:
        Image();
        Image(const ImageDescriptor& descriptor);
        ~Image();

        void write(uint16_t x, uint16_t y, 
            uint16_t width, uint16_t height, size_t offset, void* data, size_t size);
        
        bgfx::TextureHandle GetHandle();
        const ImageDescriptor& GetDescriptor() const;

        static bgfx::TextureFormat::Enum FromHPLTextureFormat(ePixelFormat format);
        static Image FromBitmap(const cBitmap& bitmap);
    private:
        bgfx::TextureHandle _handle = BGFX_INVALID_HANDLE;
        ImageDescriptor _descriptor = ImageDescriptor();
    };
} // namespace hpl