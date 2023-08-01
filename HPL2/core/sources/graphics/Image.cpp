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
#include <graphics/Bitmap.h>
#include <vector>

namespace hpl
{


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
} // namespace hpl
