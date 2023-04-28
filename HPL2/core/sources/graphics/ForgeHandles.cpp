
#include "graphics/ForgeHandles.h"
#include "graphics/Bitmap.h"

namespace hpl {
    void ForgeCmdHandle::Free() {
        if (m_handle) {
            ASSERT(m_renderer && "Renderer is null");
            removeCmd(m_renderer, m_handle);
        }
    }

    void ForgeTextureHandle::Free() {
        if (m_handle) {
            removeResource(m_handle);
        }
    }

    void ForgeBufferHandle::Free() {
        if (m_handle) {
            removeResource(m_handle);
        }
    }

    void ForgeDescriptorSet::Free() {
        if (m_handle) {
            ASSERT(m_renderer && "Renderer is null");
            removeDescriptorSet(m_renderer, m_handle);
        }
    }

    TinyImageFormat ForgeTextureHandle::FromHPLPixelFormat(ePixelFormat format) {
        switch(format) {
            case ePixelFormat_Alpha:
                return TinyImageFormat_A8_UNORM;
            case ePixelFormat_Luminance:
                return TinyImageFormat_R8_UNORM;
            case ePixelFormat_LuminanceAlpha:
                return TinyImageFormat_R8G8_UNORM;
            case ePixelFormat_RGB:
                return TinyImageFormat_R8G8B8_UNORM;
            case ePixelFormat_RGBA:
                return TinyImageFormat_R8G8B8A8_UNORM;
            case ePixelFormat_BGRA:
                return TinyImageFormat_B8G8R8A8_UNORM;
            case ePixelFormat_DXT1:
                return TinyImageFormat_DXBC1_RGBA_UNORM;
                // return bgfx::TextureFormat::BC1;
            case ePixelFormat_DXT2:
                return TinyImageFormat_DXBC2_UNORM;
            case ePixelFormat_DXT3:
                return TinyImageFormat_DXBC3_UNORM;
            case ePixelFormat_DXT4:
                return TinyImageFormat_DXBC4_UNORM;
            case ePixelFormat_DXT5: 
                return TinyImageFormat_DXBC5_UNORM;
            case ePixelFormat_Depth16:
                return TinyImageFormat_D16_UNORM;
            case ePixelFormat_Depth24:
                return TinyImageFormat_D24_UNORM_S8_UINT;
                // return bgfx::TextureFormat::D24;
            case ePixelFormat_Depth32:
                return TinyImageFormat_D32_SFLOAT;
            case ePixelFormat_Alpha16:
                return TinyImageFormat_R16_UNORM;
                // return bgfx::TextureFormat::R16F;
            case ePixelFormat_Luminance16:
                return TinyImageFormat_R16_UNORM;
                // return bgfx::TextureFormat::R16F;
            case ePixelFormat_LuminanceAlpha16:
                return TinyImageFormat_R16G16_UNORM;
                // return bgfx::TextureFormat::RG16F;
            case ePixelFormat_RGBA16:
                return TinyImageFormat_R16G16B16A16_UNORM;
                // return bgfx::TextureFormat::RGBA16F;
            case ePixelFormat_Alpha32:
                return TinyImageFormat_R32_SFLOAT;
                // return bgfx::TextureFormat::R32F;
            case ePixelFormat_Luminance32:
                return TinyImageFormat_R32_SFLOAT;
                // return bgfx::TextureFormat::R32F;
            case ePixelFormat_LuminanceAlpha32:
                return TinyImageFormat_R32G32_SFLOAT;
                // return bgfx::TextureFormat::RG32F;
            case ePixelFormat_RGBA32:
                return TinyImageFormat_R32G32B32A32_SFLOAT;
                // return bgfx::TextureFormat::RGBA32F;
            case ePixelFormat_RGB16:
                return TinyImageFormat_R16G16B16_UNORM;
                // return bgfx::TextureFormat::BC6H;
            case ePixelFormat_BGR:
                return TinyImageFormat_B8G8R8_UNORM;
                // return bgfx::TextureFormat::RGB8; // this is not supported by bgfx so we swap it under Image::InitializeFromBitmap
            default:
                ASSERT(false && "Unsupported texture format");
                break;
        }
        return TinyImageFormat_UNDEFINED;
    }


    ForgeTextureHandle ForgeTextureHandle::LoadFromHPLBitmap(cBitmap& bitmap, const BitmapLoadOptions& options) {
        ForgeTextureHandle handle;
        SyncToken token = {};

        TextureDesc desc{};
        desc.mWidth = bitmap.GetWidth();
        desc.mHeight = bitmap.GetHeight();
        desc.mDepth = bitmap.GetDepth();
        desc.mMipLevels = options.m_useMipmaps ? bitmap.GetNumOfMipMaps() : 1;
        desc.mArraySize = options.m_useArray ? bitmap.GetNumOfImages() : 1;
        desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        if(options.m_useCubeMap) {
            desc.mDescriptors |= DESCRIPTOR_TYPE_TEXTURE_CUBE;
            if(options.m_useArray) {
                ASSERT(bitmap.GetNumOfImages() % 6 == 0 && "Cube map array must have a multiple of 6 images");
                desc.mArraySize = bitmap.GetNumOfImages();
            } else {
                ASSERT(bitmap.GetNumOfImages() == 6 && "Cube map must have 6 images");
                desc.mArraySize = 6;
            }
        }
    
        TextureLoadDesc textureLoadDesc = {};
        textureLoadDesc.ppTexture = &handle.m_handle;
        textureLoadDesc.pDesc = &desc;
        addResource(&textureLoadDesc, &token);

        for(uint32_t arrIndex = 0; arrIndex < desc.mArraySize; arrIndex++) {
            for(uint32_t mipLevel = 0; mipLevel < desc.mMipLevels; mipLevel++) {
                TextureUpdateDesc update = {handle.m_handle, mipLevel, arrIndex};
                const auto& input = bitmap.GetData(arrIndex, mipLevel);
                auto data = std::span<unsigned char>(input->mpData, static_cast<size_t>(input->mlSize));
                beginUpdateResource(&update);
                for (uint32_t z = 0; z < desc.mDepth; ++z)
                {
                    uint8_t* dstData = update.pMappedData + update.mDstSliceStride * z;
                    auto srcData = data.begin() + update.mSrcSliceStride * z;
                    for (uint32_t r = 0; r < update.mRowCount; ++r) {
                        std::memcpy(dstData + r * update.mDstRowStride, &srcData[r * update.mSrcRowStride], update.mSrcRowStride);
                    }
                }
                endUpdateResource(&update, &token);
            }
        }
        handle.Initialize();
        waitForToken(&token);
        return handle;
    }
    ForgeTextureHandle ForgeTextureHandle::CreateCubemapFromHPLBitmaps(const std::span<cBitmap*> bitmaps, const BitmapCubmapLoadOptions& options) {
        ForgeTextureHandle handle;
        ASSERT(bitmaps.size() == 6 && "Cubemap must have 6 bitmaps");
        SyncToken token = {};
        TextureDesc desc{};
        desc.mWidth = bitmaps[0]->GetWidth();
        desc.mHeight = bitmaps[0]->GetHeight();
        desc.mDepth = bitmaps[0]->GetDepth();
        desc.mFormat = FromHPLPixelFormat(bitmaps[0]->GetPixelFormat());
        desc.mMipLevels = options.m_useMipmaps ? bitmaps[0]->GetNumOfMipMaps() : 1;
        desc.mArraySize = 6;
        desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_TEXTURE_CUBE;

        TextureLoadDesc textureLoadDesc = {};
        textureLoadDesc.ppTexture = &handle.m_handle;
        textureLoadDesc.pDesc = &desc;
        addResource(&textureLoadDesc, &token);

        for(auto& bitmap : bitmaps) {
            ASSERT((options.m_useMipmaps || (bitmap->GetNumOfMipMaps() == desc.mMipLevels)) && "All bitmaps must have the same number of mipmaps");
            ASSERT(bitmap->GetWidth() == desc.mWidth && "All bitmaps must have the same width");
            ASSERT(bitmap->GetHeight() == desc.mHeight && "All bitmaps must have the same height");
            ASSERT(bitmap->GetDepth() == desc.mDepth && "All bitmaps must have the same depth");
            for(uint32_t arrIndex = 0; arrIndex < desc.mArraySize; arrIndex++) {
                for(uint32_t mipLevel = 0; mipLevel < desc.mMipLevels; mipLevel++) {
                    TextureUpdateDesc update = {handle.m_handle, mipLevel, arrIndex};
                    const auto& input = bitmap->GetData(arrIndex, mipLevel);
                    auto data = std::span<unsigned char>(input->mpData, static_cast<size_t>(input->mlSize));
                    beginUpdateResource(&update);
                    for (uint32_t z = 0; z < desc.mDepth; ++z)
                    {
                        uint8_t* dstData = update.pMappedData + update.mDstSliceStride * z;
                        auto srcData = data.begin() + update.mSrcSliceStride * z;
                        for (uint32_t r = 0; r < update.mRowCount; ++r) {
                            std::memcpy(dstData + r * update.mDstRowStride, &srcData[r * update.mSrcRowStride], update.mSrcRowStride);
                        }
                    }
                    endUpdateResource(&update, &token);
                }
            }
        }

        return handle;
    }

} // namespace hpl