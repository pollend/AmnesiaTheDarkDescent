#include <graphics/Image.h>
#include "bgfx/bgfx.h"
#include "bgfx/defines.h"
#include <cstdint>
#include <bx/debug.h>
#include <graphics/Bitmap.h>
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

    ImageDescriptor ImageDescriptor::CreateFromBitmap(const cBitmap& bitmap) {
        ImageDescriptor descriptor;
        BX_ASSERT(bitmap.GetNumOfImages() == 1, "Only single image is supported at the moment");
        descriptor.format = Image::FromHPLTextureFormat(bitmap.GetPixelFormat());
        descriptor.m_width = bitmap.GetWidth();
        descriptor.m_height = bitmap.GetHeight();
        descriptor.m_depth = bitmap.GetDepth();
        descriptor.m_hasMipMaps = bitmap.GetNumOfMipMaps() > 1;
        return descriptor;
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

    ImageDescriptor ImageDescriptor::CreateTexture2D(const char* name, uint16_t width, uint16_t height, bool hasMipMaps, bgfx::TextureFormat::Enum format) {
        ImageDescriptor desc = ImageDescriptor::CreateTexture2D(width, height, hasMipMaps, format);
        desc.m_name = name;
        return desc;
    }

    ImageDescriptor ImageDescriptor::CreateTexture3D(const char* name, uint16_t width, uint16_t height, uint16_t depth, bool hasMipMaps, bgfx::TextureFormat::Enum format) {
        ImageDescriptor desc = ImageDescriptor::CreateTexture3D(width, height, depth, hasMipMaps, format);
        desc.m_name = name;
        return desc;
    }

    Image::Image() :
        iResourceBase("", _W(""), 0)
    {
    }

    Image::Image(const tString& asName, const tWString& asFullPath):
        iResourceBase(asName, asFullPath, 0)
    {
    }

    bool Image::Reload() {
        return false;
    }
    
    void Image::Unload() {
    }

    void Image::Destroy() {
    }

    Image::Image(Image&& other) :
        iResourceBase(other.GetName(), other.GetFullPath(), 0) {
        m_handle = other.m_handle;
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
        other.m_handle = BGFX_INVALID_HANDLE;
    }

    void Image::Initialize(const ImageDescriptor& descriptor, const bgfx::Memory* mem) {
        BX_ASSERT(!bgfx::isValid(m_handle), "RenderTarget already initialized");
        m_width = descriptor.m_width;
        m_height = descriptor.m_height;

        uint64_t flags = (descriptor.m_configuration.m_uClamp ? BGFX_SAMPLER_U_CLAMP : 0) |
            (descriptor.m_configuration.m_vClamp ? BGFX_SAMPLER_V_CLAMP : 0) |
            (descriptor.m_configuration.m_wClamp ? BGFX_SAMPLER_W_CLAMP : 0) |
            [&] () -> uint64_t {
                switch(descriptor.m_configuration.m_rt) {
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

        if (descriptor.m_depth > 1)
        {
            m_handle = bgfx::createTexture3D(
                descriptor.m_width,
                descriptor.m_height,
                descriptor.m_depth,
                descriptor.m_hasMipMaps,
                descriptor.format,
                flags,
                mem);
        }
        else
        {
            m_handle = bgfx::createTexture2D(
                descriptor.m_width, 
                descriptor.m_height, 
                descriptor.m_hasMipMaps, 
                descriptor.m_arraySize, 
                descriptor.format, 
                flags, 
                mem);
        }
        if(descriptor.m_name) {
            bgfx::setName(m_handle, descriptor.m_name);
        }
    }
    
    void Image::Invalidate() {
        if (bgfx::isValid(m_handle))
        {
            bgfx::destroy(m_handle);
        }
        m_handle = BGFX_INVALID_HANDLE;
    }

    // const ImageDescriptor& Image::GetDescriptor() const
    // {
    //     return m_descriptor;
    // }

    bgfx::TextureFormat::Enum Image::FromHPLTextureFormat(ePixelFormat format)
    {
        switch (format)
        {
        case ePixelFormat_Alpha:
            return bgfx::TextureFormat::A8;
        case ePixelFormat_Luminance:
            return bgfx::TextureFormat::R8;
        case ePixelFormat_LuminanceAlpha:
            return bgfx::TextureFormat::RG8;
        case ePixelFormat_RGB:
            return bgfx::TextureFormat::RGB8;
        case ePixelFormat_RGBA:
            return bgfx::TextureFormat::RGBA8;
        case ePixelFormat_BGR:
            break;
        case ePixelFormat_BGRA:
            return bgfx::TextureFormat::BGRA8;
        case ePixelFormat_DXT1:
            return bgfx::TextureFormat::BC1;
        case ePixelFormat_DXT2:
        case ePixelFormat_DXT3:
            return bgfx::TextureFormat::BC2;
        case ePixelFormat_DXT4:
        case ePixelFormat_DXT5: // DXT4 and DXT5 are the same
            return bgfx::TextureFormat::BC3;
        case ePixelFormat_Depth16:
            return bgfx::TextureFormat::D16;
        case ePixelFormat_Depth24:
            return bgfx::TextureFormat::D24;
        case ePixelFormat_Depth32:
            return bgfx::TextureFormat::D32;
        case ePixelFormat_Alpha16:
            return bgfx::TextureFormat::R16F;
        case ePixelFormat_Luminance16:
            return bgfx::TextureFormat::R16F;
        case ePixelFormat_LuminanceAlpha16:
            return bgfx::TextureFormat::RG16F;
        case ePixelFormat_RGB16:
            break;
        case ePixelFormat_RGBA16:
            return bgfx::TextureFormat::RGBA16F;
            break;
        case ePixelFormat_Alpha32:
            return bgfx::TextureFormat::R32F;
            break;
        case ePixelFormat_Luminance32:
            return bgfx::TextureFormat::R32F;
        case ePixelFormat_LuminanceAlpha32:
            return bgfx::TextureFormat::RG32F;
        case ePixelFormat_RGB32:
            break;
        case ePixelFormat_RGBA32:
            return bgfx::TextureFormat::RGBA32F;
        default:
            break;
        }
        return bgfx::TextureFormat::Unknown;
    }

    void Image::InitializeFromBitmap(Image& image, cBitmap& bitmap, const ImageDescriptor& desc) {

        auto data = bitmap.GetData(0, 0);
        image.Initialize(desc, bgfx::copy(data->mpData, data->mlSize));
    }

    // void Image::loadFromBitmap(Image& image, cBitmap& bitmap) {
    //     ImageDescriptor descriptor;
    //     BX_ASSERT(bitmap.GetNumOfImages() == 1, "Only single image is supported at the moment");
    //     descriptor.format = Image::FromHPLTextureFormat(bitmap.GetPixelFormat());
    //     descriptor.m_width = bitmap.GetWidth();
    //     descriptor.m_height = bitmap.GetHeight();
    //     descriptor.m_depth = bitmap.GetDepth();
    //     auto data = bitmap.GetData(0, 0);
    //     image.Initialize(descriptor, bgfx::copy(data->mpData, data->mlSize));
    // }


    bgfx::TextureHandle Image::GetHandle() const
    {
        return m_handle;
    }
} // namespace hpl