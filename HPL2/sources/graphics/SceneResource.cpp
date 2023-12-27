#include "graphics/SceneResource.h"
#include "graphics/Material.h"
#include "tinyimageformat_query.h"
#include <cstdint>

namespace hpl::resource  {

    uint32_t textureFilterIdx(TextureAntistropy anisotropy, eTextureWrap wrap, eTextureFilter filter) {
        const uint32_t anisotropyGroup =
            (static_cast<uint32_t>(eTextureFilter_LastEnum) * static_cast<uint32_t>(eTextureWrap_LastEnum)) *
            static_cast<uint32_t>(anisotropy);
        return anisotropyGroup +
            ((static_cast<uint32_t>(wrap) * static_cast<uint32_t>(eTextureFilter_LastEnum)) + static_cast<uint32_t>(filter));
    }

    uint32_t textureFilterNonAnistropyIdx(eTextureWrap wrap, eTextureFilter filter) {
        return ((static_cast<uint32_t>(wrap) * static_cast<uint32_t>(eTextureFilter_LastEnum)) + static_cast<uint32_t>(filter));
    }

    std::array<SharedSampler, resource::MaterialSceneSamplersCount> createSceneSamplers(Renderer* renderer) {
        std::array<SharedSampler, resource::MaterialSceneSamplersCount> result;
        for (size_t antistropy = 0; antistropy < static_cast<uint8_t>(TextureAntistropy::Antistropy_Count); antistropy++) {
            for (size_t textureWrap = 0; textureWrap < eTextureWrap_LastEnum; textureWrap++) {
                for (size_t textureFilter = 0; textureFilter < eTextureFilter_LastEnum; textureFilter++) {
                    uint32_t batchID = hpl::resource::textureFilterIdx(
                        static_cast<TextureAntistropy>(antistropy),
                        static_cast<eTextureWrap>(textureWrap),
                        static_cast<eTextureFilter>(textureFilter));
                    result[batchID].Load(renderer, [&](Sampler** sampler) {
                        SamplerDesc samplerDesc = {};
                        switch (textureWrap) {
                        case eTextureWrap_Repeat:
                            samplerDesc.mAddressU = ADDRESS_MODE_REPEAT;
                            samplerDesc.mAddressV = ADDRESS_MODE_REPEAT;
                            samplerDesc.mAddressW = ADDRESS_MODE_REPEAT;
                            break;
                        case eTextureWrap_Clamp:
                            samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
                            samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
                            samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
                            break;
                        case eTextureWrap_ClampToBorder:
                            samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_BORDER;
                            samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_BORDER;
                            samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_BORDER;
                            break;
                        default:
                            ASSERT(false && "Invalid wrap mode");
                            break;
                        }
                        switch (textureFilter) {
                        case eTextureFilter_Nearest:
                            samplerDesc.mMinFilter = FilterType::FILTER_NEAREST;
                            samplerDesc.mMagFilter = FilterType::FILTER_NEAREST;
                            samplerDesc.mMipMapMode = MipMapMode::MIPMAP_MODE_NEAREST;
                            break;
                        case eTextureFilter_Bilinear:
                            samplerDesc.mMinFilter = FilterType::FILTER_LINEAR;
                            samplerDesc.mMagFilter = FilterType::FILTER_LINEAR;
                            samplerDesc.mMipMapMode = MipMapMode::MIPMAP_MODE_NEAREST;
                            break;
                        case eTextureFilter_Trilinear:
                            samplerDesc.mMinFilter = FilterType::FILTER_LINEAR;
                            samplerDesc.mMagFilter = FilterType::FILTER_LINEAR;
                            samplerDesc.mMipMapMode = MipMapMode::MIPMAP_MODE_LINEAR;
                            break;
                        default:
                            ASSERT(false && "Invalid filter");
                            break;
                        }
                        switch (static_cast<TextureAntistropy>(antistropy)) {
                        case TextureAntistropy::Antistropy_8:
                            samplerDesc.mMaxAnisotropy = 8.0f;
                            break;
                        case TextureAntistropy::Antistropy_16:
                            samplerDesc.mMaxAnisotropy = 16.0f;
                            break;
                        default:
                            break;
                        }
                        addSampler(renderer, &samplerDesc, sampler);
                        return true;
                    });
                }
            }
        }
        return result;
    }

}
