#pragma once

#include <absl/types/span.h>
#include <graphics/Enum.h>
#include <cstdint>
#include <graphics/GraphicsTypes.h>
#include <bgfx/bgfx.h>
#include <memory>

namespace hpl
{
     struct ImageDescriptor {
        ImageDescriptor();
        ImageDescriptor(const ImageDescriptor& desc);

        uint16_t m_width = 0;
        uint16_t m_height = 0;
        uint16_t m_depth = 1;
        uint16_t m_arraySize = 1;
        bool m_hasMipMaps = false;

        union {
            struct {
                bool m_uClamp : 1;
                bool m_vClamp : 1;
                bool m_wClamp : 1;
                RTType m_rt: 2;
            };
            uint64_t m_settings = 0;
        } m_configuration;

        bgfx::TextureFormat::Enum format;

        static ImageDescriptor CreateTexture2D(uint16_t width, uint16_t height, bool hasMipMaps, bgfx::TextureFormat::Enum format);
        static ImageDescriptor CreateTexture3D(uint16_t width, uint16_t height, uint16_t depth, bool hasMipMaps, bgfx::TextureFormat::Enum format);
    };

    class Image
    {
    public:
        Image();
        ~Image();

        Image(const ImageDescriptor& desc);
        Image(Image&& other);
        Image(const Image& other) = delete;
        
        Image& operator=(const Image& other) = delete;
        void operator=(Image&& other);

        void Initialize(const ImageDescriptor& descriptor,const bgfx::Memory* mem = nullptr);
        void Invalidate();

        bgfx::TextureHandle GetHandle() const;
        const ImageDescriptor& GetDescriptor() const;

        static bgfx::TextureFormat::Enum FromHPLTextureFormat(ePixelFormat format);

    private:
        bgfx::TextureHandle m_handle = BGFX_INVALID_HANDLE;
        ImageDescriptor m_descriptor = ImageDescriptor();
    };
} // namespace hpl