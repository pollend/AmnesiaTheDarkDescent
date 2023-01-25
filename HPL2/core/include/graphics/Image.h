#pragma once

#include "absl/strings/string_view.h"
#include "graphics/Bitmap.h"
#include "math/MathTypes.h"
#include "resources/ResourceBase.h"
#include "system/SystemTypes.h"
#include <absl/types/span.h>
#include <graphics/Enum.h>
#include <cstdint>
#include <graphics/GraphicsTypes.h>
#include <bgfx/bgfx.h>
#include <memory>
#include <string>
#include <engine/RTTI.h>

namespace hpl
{

    class cBitmap;
     struct ImageDescriptor {
        ImageDescriptor();
        ImageDescriptor(const ImageDescriptor& desc);

        uint16_t m_width = 0;
        uint16_t m_height = 0;
        uint16_t m_depth = 1;
        uint16_t m_arraySize = 1;
        bool m_hasMipMaps = false;
        bool m_isCubeMap = false;
        const char* m_name = nullptr;

        union {
            struct {
                bool m_uClamp : 1;
                bool m_vClamp : 1;
                bool m_wClamp : 1;
                RTType m_rt: 3;
            };
            uint64_t m_settings = 0;
        } m_configuration;

        bgfx::TextureFormat::Enum format;

        static ImageDescriptor CreateTexture2D(uint16_t width, uint16_t height, bool hasMipMaps, bgfx::TextureFormat::Enum format);
        static ImageDescriptor CreateTexture3D(uint16_t width, uint16_t height, uint16_t depth, bool hasMipMaps, bgfx::TextureFormat::Enum format);
        static ImageDescriptor CreateFromBitmap(const cBitmap& bitmap);

        static ImageDescriptor CreateTexture2D(const char* name, uint16_t width, uint16_t height, bool hasMipMaps, bgfx::TextureFormat::Enum format);
        static ImageDescriptor CreateTexture3D(const char* name, uint16_t width, uint16_t height, uint16_t depth, bool hasMipMaps, bgfx::TextureFormat::Enum format);
    };

    class Image : public iResourceBase
    {
        HPL_RTTI_IMPL_CLASS(iResourceBase, Image, "{d9cd842a-c76b-4261-879f-53f1baa5ff7c}")

    public:
        Image();
        Image(const tString& asName, const tWString& asFullPath);

        ~Image();
        Image(Image&& other);
        Image(const Image& other) = delete;
        
        Image& operator=(const Image& other) = delete;
        void operator=(Image&& other);

        void Initialize(const ImageDescriptor& descriptor,const bgfx::Memory* mem = nullptr);
        void Invalidate();

        bgfx::TextureHandle GetHandle() const;

        static bgfx::TextureFormat::Enum FromHPLTextureFormat(ePixelFormat format);

        static void InitializeFromBitmap(Image& image, cBitmap& bitmap, const ImageDescriptor& desc);
        static void InitializeCubemapFromBitmaps(Image& image, const absl::Span<cBitmap*> bitmaps, const ImageDescriptor& desc);
        
        virtual bool Reload() override;
        virtual void Unload() override;
        virtual void Destroy() override;

        uint16_t GetWidth() const {return m_width;}
        uint16_t GetHeight() const {return m_height;}

        cVector2l GetImageSize() const {return cVector2l(m_width, m_height);}
    private:

        uint16_t m_width = 0;
        uint16_t m_height = 0;

        bgfx::TextureHandle m_handle = BGFX_INVALID_HANDLE;
    };

} // namespace hpl