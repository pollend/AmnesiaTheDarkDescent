#include <graphics/Image.h>
#include "bgfx/bgfx.h"
#include <cstdint>


namespace hpl
{

    ImageDescriptor::ImageDescriptor(const ImageDescriptor& desc)
        : width(desc.width)
        , height(desc.height)
        , format(desc.format)
    {
    }
    ImageDescriptor::ImageDescriptor()
        : width(0)
        , height(0)
        , format(bgfx::TextureFormat::Unknown)
    {
    }

    Image::Image()
    {
    }

    Image::Image(const ImageDescriptor& descriptor)
        : _descriptor(descriptor)
    {
        _handle = bgfx::createTexture2D(descriptor.width, descriptor.height, false, 1, descriptor.format, BGFX_TEXTURE_RT);
    }

    void Image::write(uint16_t x, uint16_t y, uint16_t width, uint16_t height, size_t offset, void* data, size_t size)
    {
        bgfx::updateTexture2D(_handle, 0, 0, x, y, width, height, bgfx::copy(data, size));
    }

    const ImageDescriptor& Image::GetDescriptor() const
    {
        return _descriptor;
    }

    Image::~Image()
    {
        if (bgfx::isValid(_handle))
        {
            bgfx::destroy(_handle);
        }
    }

    static bgfx::TextureFormat::Enum FromHPLTextureFormat(ePixelFormat format)
    {
        switch (format)
        {
        case ePixelFormat_Alpha:
            break;
        case ePixelFormat_Luminance:
            break;
        case ePixelFormat_LuminanceAlpha:
            break;
        case ePixelFormat_RGB:
            return bgfx::TextureFormat::RGB8;
        case ePixelFormat_RGBA:
            return bgfx::TextureFormat::RGBA8;
        case ePixelFormat_BGR:
            break;
        case ePixelFormat_BGRA:
            break;
        case ePixelFormat_DXT1:
            return bgfx::TextureFormat::BC1;
        case ePixelFormat_DXT2:
            return bgfx::TextureFormat::BC2;
        case ePixelFormat_DXT3:
            return bgfx::TextureFormat::BC3;
        case ePixelFormat_DXT4:
            return bgfx::TextureFormat::BC4;
        case ePixelFormat_DXT5:
            return bgfx::TextureFormat::BC5;
        case ePixelFormat_Depth16:
            return bgfx::TextureFormat::D16;
        case ePixelFormat_Depth24:
            return bgfx::TextureFormat::D24;
        case ePixelFormat_Depth32:
            return bgfx::TextureFormat::D32;
        case ePixelFormat_Alpha16:
            break;
        case ePixelFormat_Luminance16:
            break;
        case ePixelFormat_LuminanceAlpha16:
            break;
        case ePixelFormat_RGB16:
            break;
        case ePixelFormat_RGBA16:
            break;
        case ePixelFormat_Alpha32:
            break;
        case ePixelFormat_Luminance32:
            break;
        case ePixelFormat_LuminanceAlpha32:
            break;
        case ePixelFormat_RGB32:
            break;
        case ePixelFormat_RGBA32:
            break;
        default:
            break;
        }
        return bgfx::TextureFormat::Unknown;
    }

    static Image FromBitmap(const cBitmap& bitmap)
    {
        ImageDescriptor descriptor;
        descriptor.format = FromHPLTextureFormat(bitmap.GetPixelFormat());
        // bitmap.GetPixelFormat()

        // bitmap.GetPixelFormat()
        // bgfx::createTexture2D()
        return Image();
    }

    bgfx::TextureHandle Image::GetHandle()
    {
        return _handle;
    }
} // namespace hpl