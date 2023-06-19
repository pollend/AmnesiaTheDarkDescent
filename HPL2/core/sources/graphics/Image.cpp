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

#include <graphics/Image.h>
#include "bgfx/bgfx.h"
#include "bgfx/defines.h"
#include <bx/debug.h>
#include <graphics/Bitmap.h>
#include <vector>

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
        // descriptor.format = Image::FromHPLTextureFormat(bitmap.GetPixelFormat());
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
        // m_textures = std::make_shared(Texture{0})
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
        m_texture = std::move(other.m_texture);
    }

    Image::~Image()
    {
    }

    void Image::operator=(Image&& other) {
        m_texture = std::move(other.m_texture);
    }

    void Image::Initialize(const ImageDescriptor& descriptor, const bgfx::Memory* mem) {
        ASSERT(false && "Deprecated");
    }

    void Image::setTextureFilter(const eTextureFilter& filter) {
        ASSERT(m_texture.IsValid());
        if(m_texture.m_handle->mMipLevels > 0)
        {
            switch(filter)
            {
            case eTextureFilter_Nearest:
                m_samplerDesc.mMinFilter = FilterType::FILTER_NEAREST;
                m_samplerDesc.mMagFilter = FilterType::FILTER_NEAREST;
                m_samplerDesc.mMipMapMode = MipMapMode::MIPMAP_MODE_NEAREST;
                break;
            case eTextureFilter_Bilinear:
                m_samplerDesc.mMinFilter = FilterType::FILTER_LINEAR;
                m_samplerDesc.mMagFilter = FilterType::FILTER_LINEAR;
                m_samplerDesc.mMipMapMode = MipMapMode::MIPMAP_MODE_NEAREST;
                break;
            case eTextureFilter_Trilinear:
                m_samplerDesc.mMinFilter = FilterType::FILTER_LINEAR;
                m_samplerDesc.mMagFilter = FilterType::FILTER_LINEAR;
                m_samplerDesc.mMipMapMode = MipMapMode::MIPMAP_MODE_LINEAR;
                break;
            default:
                ASSERT(false && "Invalid filter");
                break;
            }
        }
        else
        {
            switch(filter)
            {
            case eTextureFilter_Nearest:
                m_samplerDesc.mMinFilter = FilterType::FILTER_NEAREST;
                m_samplerDesc.mMagFilter = FilterType::FILTER_NEAREST;
                m_samplerDesc.mMipMapMode = MipMapMode::MIPMAP_MODE_NEAREST;
                break;
            case eTextureFilter_Bilinear:
            case eTextureFilter_Trilinear:
                m_samplerDesc.mMinFilter = FilterType::FILTER_LINEAR;
                m_samplerDesc.mMagFilter = FilterType::FILTER_LINEAR;
                m_samplerDesc.mMipMapMode = MipMapMode::MIPMAP_MODE_NEAREST;
                break;
            default:
                ASSERT(false && "Invalid filter");
                break;
            }

        }
    }

    void Image::setWrapMode(const eTextureWrap& wrap) {
        ASSERT(m_texture.IsValid());
        switch(wrap) {
            case eTextureWrap_Repeat:
                m_samplerDesc.mAddressU = ADDRESS_MODE_REPEAT;
                m_samplerDesc.mAddressV = ADDRESS_MODE_REPEAT;
                m_samplerDesc.mAddressW = ADDRESS_MODE_REPEAT;
                break;
            case eTextureWrap_Clamp:
            case eTextureWrap_ClampToEdge:
                m_samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
                m_samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
                m_samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
                break;
            case eTextureWrap_ClampToBorder:
                m_samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_BORDER;
                m_samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_BORDER;
                m_samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_BORDER;
                break;
            default:
                ASSERT(false && "Invalid wrap mode");
                break;
        }
    }

    void Image::Invalidate() {
        // if (bgfx::isValid(m_handle))
        // {
        //     bgfx::destroy(m_handle);
        // }
        // m_handle = BGFX_INVALID_HANDLE;
        // m_texture.TryFree();
    }

    void Image::InitializeCubemapFromBitmaps(Image& image, const std::span<cBitmap*> bitmaps, const ImageDescriptor& desc) {
        BX_ASSERT(bitmaps.size() == 6, "Cubemap must have 6 bitmaps");
        size_t size = 0;
        for(auto& bitmap : bitmaps) {
            BX_ASSERT(bitmap->GetNumOfImages() == 1, "Only single image bitmaps are supported");
            if(desc.m_hasMipMaps) {
                for(auto mipIndex = 0; mipIndex < bitmap->GetNumOfMipMaps(); ++mipIndex) {
                    size += bitmap->GetData(0, mipIndex)->mlSize;
                }
            } else {
                size += bitmap->GetData(0, 0)->mlSize;
            }
        }
        auto* memory = bgfx::alloc(size);
        size_t offset = 0;
        if(desc.m_hasMipMaps) {
            for(auto& bitmap : bitmaps) {
                for(auto mipIndex = 0; mipIndex < bitmap->GetNumOfMipMaps(); ++mipIndex) {
                    auto data = bitmap->GetData(0, mipIndex);
                    std::copy(data->mpData, data->mpData + data->mlSize, memory->data + offset);
                    offset += data->mlSize;
                }
            }
        } else {
            for(auto& bitmap : bitmaps) {
                auto data = bitmap->GetData(0, 0);
                std::copy(data->mpData, data->mpData + data->mlSize, memory->data + offset);
                offset += data->mlSize;
            }
        }
        image.Initialize(desc, memory);
    }

    void Image::InitializeFromBitmap(Image& image, cBitmap& bitmap, const ImageDescriptor& desc) {

        auto copyDataToChunk = [&](unsigned char* m_begin, unsigned char* m_end, unsigned char* dest) {

            // BGR is not supported by bgfx, so we need to convert it to RGB
            if(bitmap.GetPixelFormat() == ePixelFormat_BGR) {
                for(auto* src = m_begin; src < m_end; src += 3) {
                    *(dest++) = src[2];
                    *(dest++) = src[1];
                    *(dest++) = src[0];
                }
            } else {
                std::copy(m_begin, m_end, dest);
            }
        };

        if(desc.m_isCubeMap) {
            BX_ASSERT(bitmap.GetNumOfImages() == 6, "Cube map must have 6 images");

            auto* memory = bgfx::alloc([&]() {
                if(desc.m_hasMipMaps) {
                    size_t size = 0;
                    for(size_t i = 0; i < bitmap.GetNumOfMipMaps(); ++i) {
                        size += bitmap.GetData(0, i)->mlSize;
                    }
                    return size * static_cast<size_t>(bitmap.GetNumOfImages());
                }
                return static_cast<size_t>(bitmap.GetNumOfImages()) * static_cast<size_t>(bitmap.GetData(0, 0)->mlSize);
            }());


            size_t offset = 0;
            if(desc.m_hasMipMaps) {
                for(auto imageIdx = 0; imageIdx < bitmap.GetNumOfImages(); ++imageIdx) {
                    for(auto mipIndex = 0; mipIndex < bitmap.GetNumOfMipMaps(); ++mipIndex) {
                        auto data = bitmap.GetData(imageIdx, mipIndex);
                        // std::copy(data->mpData, data->mpData + data->mlSize, memory->data + offset);
                        copyDataToChunk(data->mpData, data->mpData + data->mlSize, memory->data + offset);
                        offset += data->mlSize;
                    }
                }
            } else {
                for(auto i = 0; i < bitmap.GetNumOfImages(); ++i) {
                    auto data = bitmap.GetData(i, 0);
                    // std::copy(data->mpData, data->mpData + data->mlSize, memory->data + offset);
                    copyDataToChunk(data->mpData, data->mpData + data->mlSize, memory->data + offset);
                    offset += data->mlSize;
                }
            }
            image.Initialize(desc, memory);
            return;
        }

        if(desc.m_hasMipMaps) {
            auto* memory = bgfx::alloc([&]() {
                size_t size = 0;
                for(size_t i = 0; i < bitmap.GetNumOfMipMaps(); ++i) {
                    size += bitmap.GetData(0, i)->mlSize;
                }
                return size;
            }());

            size_t offset = 0;
            for(auto i = 0; i < bitmap.GetNumOfMipMaps(); ++i) {
                auto data = bitmap.GetData(0, i);
                // std::copy(data->mpData, data->mpData + data->mlSize, memory->data + offset);
                copyDataToChunk(data->mpData, data->mpData + data->mlSize, memory->data + offset);
                offset += data->mlSize;
            }
            image.Initialize(desc, memory);
            return;
        }

        auto data = bitmap.GetData(0, 0);
        auto* memory = bgfx::alloc(data->mlSize);
        copyDataToChunk(data->mpData, data->mpData + data->mlSize, memory->data);
        image.Initialize(desc, memory);
    }

    bgfx::TextureHandle Image::GetHandle() const
    {
        ASSERT(false && "Not implemented");
        return bgfx::TextureHandle{};
        // return m_handle;
    }
} // namespace hpl
