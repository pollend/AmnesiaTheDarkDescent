#include <graphics/Image.h>
#include "bgfx/bgfx.h"
#include "bgfx/defines.h"
#include <cstdint>
#include <bx/debug.h>
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

    ImageDescriptor ImageDescriptor::CreateTexture3D(uint16_t width, uint16_t height, uint16_t depth, bool hasMipMaps, bgfx::TextureFormat::Enum format) {
        ImageDescriptor desc;
        desc.m_width = width;
        desc.m_height = height;
        desc.m_depth = depth;
        desc.m_hasMipMaps = hasMipMaps;
        desc.format = format;
        return desc;
    }

    Image::Image()
    {
    }

    Image::Image(const ImageDescriptor& desc) {
        Initialize(desc);
    }

    Image::Image(Image&& other) {
        m_handle = other.m_handle;
        m_descriptor = other.m_descriptor;
        other.m_handle = BGFX_INVALID_HANDLE;
    }

    Image::~Image()
    {
        if (bgfx::isValid(m_handle))
        {
            bgfx::destroy(m_handle);
        }
    }

    void Image::operator=(Image&& other) {
        m_handle = other.m_handle;
        m_descriptor = other.m_descriptor;
        other.m_handle = BGFX_INVALID_HANDLE;
    }

    void Image::Initialize(const ImageDescriptor& descriptor, const bgfx::Memory* mem) {
        BX_ASSERT(!bgfx::isValid(m_handle), "RenderTarget already initialized");

        m_descriptor = descriptor;
        uint64_t flags = (m_descriptor.m_configuration.m_uClamp ? BGFX_SAMPLER_U_CLAMP : 0) |
            (m_descriptor.m_configuration.m_vClamp ? BGFX_SAMPLER_V_CLAMP : 0) |
            (m_descriptor.m_configuration.m_wClamp ? BGFX_SAMPLER_W_CLAMP : 0) |
            [&] () -> uint64_t {
                switch(m_descriptor.m_configuration.m_rt) {
                    case RTType::RT_Write:
                        return BGFX_TEXTURE_RT;
                    case RTType::RT_WriteOnly:
                        return BGFX_TEXTURE_RT_WRITE_ONLY;
                    case RTType::RT_MSAA_X2:
                        return BGFX_TEXTURE_RT_MSAA_X2;
                    case RTType::RT_MSAA_X4:
                        return BGFX_TEXTURE_RT_MSAA_X4;
                    case RTType::RT_MSAA_X8:
                        return BGFX_TEXTURE_RT_MSAA_X8;
                    case RTType::RT_MSAA_X16:
                        return BGFX_TEXTURE_RT_MSAA_X16;
                    default:
                        break;
                }
                return 0;
            }();

        if (m_descriptor.m_depth > 1)
        {
            m_handle = bgfx::createTexture3D(
                m_descriptor.m_width,
                m_descriptor.m_height,
                m_descriptor.m_depth,
                m_descriptor.m_hasMipMaps,
                m_descriptor.format,
                flags,
                mem);
        }
        else
        {
            m_handle = bgfx::createTexture2D(
                m_descriptor.m_width, 
                m_descriptor.m_height, 
                m_descriptor.m_hasMipMaps, 
                m_descriptor.m_arraySize, 
                m_descriptor.format, 
                flags, 
                mem);
        }
    }
    
    void Image::Invalidate() {
        if (bgfx::isValid(m_handle))
        {
            bgfx::destroy(m_handle);
        }
        m_handle = BGFX_INVALID_HANDLE;
    }

    const ImageDescriptor& Image::GetDescriptor() const
    {
        return m_descriptor;
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
        return m_handle;
    }
} // namespace hpl