/**
 * Copyright 2023 Michael Pollind
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "graphics/Bitmap.h"
#include "graphics/ForgeRenderer.h"
#include "math/MathTypes.h"
#include "system/SystemTypes.h"
#include <bgfx/bgfx.h>
#include <cstdint>
#include <engine/RTTI.h>
#include <graphics/Enum.h>
#include <graphics/GraphicsTypes.h>
#include <memory>
#include <resources/ResourceBase.h>
#include <span>
#include <string>

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include <FixPreprocessor.h>
namespace hpl {

    class cBitmap;
    struct ImageDescriptor {
        [[deprecated("Deprecating BGFX")]] ImageDescriptor();

        [[deprecated("Deprecating BGFX")]] ImageDescriptor(const ImageDescriptor& desc);

        uint16_t m_width = 0;
        uint16_t m_height = 0;
        uint16_t m_depth = 1;
        uint16_t m_arraySize = 1;
        bool m_hasMipMaps = false;
        bool m_isCubeMap = false;
        const char* m_name = nullptr;

        union {
            struct {
                bool m_computeWrite : 1;
                WrapMode m_UWrap : 3;
                WrapMode m_VWrap : 3;
                WrapMode m_WWrap : 3;
                RTType m_rt : 3;
                DepthTest m_comparsion : 3;
                FilterType m_minFilter : 2;
                FilterType m_magFilter : 2;
                FilterType m_mipFilter : 2;
            };
            uint64_t m_settings = 0;
        } m_configuration;

        bgfx::TextureFormat::Enum format;

        static ImageDescriptor CreateTexture2D(uint16_t width, uint16_t height, bool hasMipMaps, bgfx::TextureFormat::Enum format);
        static ImageDescriptor CreateTexture3D(
            uint16_t width, uint16_t height, uint16_t depth, bool hasMipMaps, bgfx::TextureFormat::Enum format);
        static ImageDescriptor CreateFromBitmap(const cBitmap& bitmap);

        static ImageDescriptor CreateTexture2D(
            const char* name, uint16_t width, uint16_t height, bool hasMipMaps, bgfx::TextureFormat::Enum format);
        static ImageDescriptor CreateTexture3D(
            const char* name, uint16_t width, uint16_t height, uint16_t depth, bool hasMipMaps, bgfx::TextureFormat::Enum format);
    };

    class Image : public iResourceBase {
        HPL_RTTI_IMPL_CLASS(iResourceBase, Image, "{d9cd842a-c76b-4261-879f-53f1baa5ff7c}")
    public:

        Image();
        Image(const tString& asName, const tWString& asFullPath);

        ~Image();
        Image(Image&& other);
        Image(const Image& other) = delete;

        Image& operator=(const Image& other) = delete;
        void operator=(Image&& other);

        inline void SetForgeTexture(ForgeTextureHandle&& handle) {
            m_texture = std::move(handle);
        }


        void Initialize(const ImageDescriptor& descriptor, const bgfx::Memory* mem = nullptr);
        void Invalidate();

        [[deprecated("Removing BGFX")]] bgfx::TextureHandle GetHandle() const;

        static void InitializeFromBitmap(Image& image, cBitmap& bitmap, const ImageDescriptor& desc);
        static void InitializeCubemapFromBitmaps(Image& image, const std::span<cBitmap*> bitmaps, const ImageDescriptor& desc);

        virtual bool Reload() override;
        virtual void Unload() override;
        virtual void Destroy() override;

        inline uint16_t GetWidth() const {
            ASSERT(m_texture.IsValid());
            return m_texture.m_handle->mWidth;
        }
        inline uint16_t GetHeight() const {
            ASSERT(m_texture.IsValid());
            return m_texture.m_handle->mHeight;
        }

        cVector2l GetImageSize() const {
            if (m_texture.IsValid()) {
                return cVector2l(m_texture.m_handle->mWidth, m_texture.m_handle->mHeight);
            }
            return cVector2l(0, 0);
        }

        inline ForgeTextureHandle& GetTexture() {
            return m_texture;
        }
        void setTextureFilter(const eTextureFilter& filter);
        void setWrapMode(const eTextureWrap& wrap);

        SamplerDesc m_samplerDesc{};
        ForgeTextureHandle m_texture;
    };

} // namespace hpl
