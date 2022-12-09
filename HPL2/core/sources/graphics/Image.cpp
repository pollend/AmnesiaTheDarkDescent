#include <graphics/Image.h>
#include "bgfx/bgfx.h"
#include <cstdint>

namespace hpl
{

    ImageDescriptor::ImageDescriptor(const ImageDescriptor& desc)
        : m_width(desc.m_width)
        , m_height(desc.m_height)
        , m_arraySize(desc.m_arraySize)
        , m_hasMipMaps(desc.m_hasMipMaps)
        , format(desc.format)
    {
    }
    ImageDescriptor::ImageDescriptor()
        : m_width(0)
        , m_height(0)
        , m_arraySize(1)
        , m_hasMipMaps(false)
        , format(bgfx::TextureFormat::Unknown)
    {
    }

    ImageDescriptor ImageDescriptor::CreateTexture2D(uint16_t width, uint16_t height, bool hasMipMaps, bgfx::TextureFormat::Enum format) {
        ImageDescriptor desc;
        desc.m_width = width;
        desc.m_height = height;
        desc.m_hasMipMaps = hasMipMaps;
        desc.format = format;
        return desc;
    }

    Image::Image()
    {
    }

    Image::~Image()
    {
        if (bgfx::isValid(_handle))
        {
            bgfx::destroy(_handle);
        }
    }

    void Image::Initialize(const ImageDescriptor& descriptor, const bgfx::Memory* mem) {
        _descriptor = descriptor;

        _handle = bgfx::createTexture2D(
            descriptor.m_width, descriptor.m_height, 
            descriptor.m_hasMipMaps, 1, descriptor.format, BGFX_TEXTURE_RT, mem);
    }
    
    void Image::Invalidate() {

    }

    // void Image::write(uint16_t x, uint16_t y, uint16_t width, uint16_t height, size_t offset, void* data, size_t size)
    // {
    //     bgfx::updateTexture2D(_handle, 0, 0, x, y, width, height, bgfx::copy(data, size));
    // }

    const ImageDescriptor& Image::GetDescriptor() const
    {
        return _descriptor;
    }


    bgfx::TextureFormat::Enum Image::FromHPLTextureFormat(ePixelFormat format)
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

    bgfx::TextureHandle Image::GetHandle() const
    {
        return _handle;
    }
} // namespace hpl