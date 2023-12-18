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

    uint32_t encodeMaterialID(hpl::MaterialID id, uint32_t handle) {
        return (static_cast<uint32_t>(id) << MaterialIdBit) & MaterialIDMask | (handle << MaterialIndexBit) & MaterialIndexMask;
    }

    void visitTextures(MaterialTypes& material, std::function<void(eMaterialTexture, uint32_t slot)> handler) {
        if (auto* item = std::get_if<DiffuseMaterial>(&material)) {
            struct {
                eMaterialTexture m_type;
                uint32_t* m_value;
            } m_textures[] = {
                { eMaterialTexture_Diffuse, &item->m_diffuseTextureIndex},
                { eMaterialTexture_NMap, &item->m_normalTextureIndex},
                { eMaterialTexture_Specular, &item->m_specularTextureIndex},
                { eMaterialTexture_Alpha, &item->m_alphaTextureIndex},
                { eMaterialTexture_Height, &item->m_heightTextureIndex },
                { eMaterialTexture_Illumination, &item->m_illuminiationTextureIndex},
                { eMaterialTexture_DissolveAlpha, &item->m_dissolveAlphaTextureIndex},
                { eMaterialTexture_CubeMapAlpha, &item->m_cubeMapAlphaTextureIndex},
            };
            for (auto& tex : m_textures) {
                if (*tex.m_value != UINT32_MAX) {
                    handler(tex.m_type, *tex.m_value);
                }
            }
        }
    }
    std::variant<DiffuseMaterial, std::monostate> createMaterial(TextureDescriptorPool& pool, cMaterial* material) {
        ASSERT(material);
        auto& descriptor = material->Descriptor();
        switch (descriptor.m_id) {
            case hpl::MaterialID::SolidDiffuse: {
                DiffuseMaterial result = {};
                struct {
                    eMaterialTexture m_type;
                    uint32_t* m_value;
                } m_textures[] = {
                    {eMaterialTexture_Diffuse, &result.m_diffuseTextureIndex},
                    {eMaterialTexture_NMap, &result.m_normalTextureIndex},
                    {eMaterialTexture_Alpha, &result.m_alphaTextureIndex},
                    {eMaterialTexture_Specular, &result.m_specularTextureIndex},
                    {eMaterialTexture_Height, &result.m_heightTextureIndex},
                    {eMaterialTexture_Illumination, &result.m_illuminiationTextureIndex},
                    {eMaterialTexture_DissolveAlpha, &result.m_dissolveAlphaTextureIndex},
                    {eMaterialTexture_CubeMapAlpha, &result.m_cubeMapAlphaTextureIndex},
                };
                for(auto& tex: m_textures) {
                    auto* image = material->GetImage(tex.m_type);
                    (*tex.m_value) = UINT32_MAX;
                    if(image) {
                        uint32_t handle = pool.request(image->GetTexture());
                        (*tex.m_value) = static_cast<uint16_t>(handle);
                    }
                }

	            const auto alphaMapImage = material->GetImage(eMaterialTexture_Alpha);
		        const auto heightMapImage = material->GetImage(eMaterialTexture_Height);

                result.m_materialConfig =
			        ((alphaMapImage && TinyImageFormat_ChannelCount(static_cast<TinyImageFormat>(alphaMapImage->GetTexture().m_handle->mFormat)) == 1) ? IsAlphaSingleChannel: 0) |
			        ((heightMapImage && TinyImageFormat_ChannelCount(static_cast<TinyImageFormat>(heightMapImage->GetTexture().m_handle->mFormat)) == 1) ? IsHeightSingleChannel : 0) |
			        (descriptor.m_solid.m_alphaDissolveFilter ? UseAlphaDissolveFilter : 0);

                result.m_samplerIndex = textureFilterNonAnistropyIdx(material->GetTextureWrap(), material->GetTextureFilter());
                result.m_heightMapScale = descriptor.m_solid.m_heightMapScale;
                result.m_heigtMapBias = descriptor.m_solid.m_heightMapBias;
                result.m_frenselBias = descriptor.m_solid.m_frenselBias;
                result.m_frenselPow = descriptor.m_solid.m_frenselPow;
                return result;
            }
            case hpl::MaterialID::Decal:
                break;
            case hpl::MaterialID::Translucent:

                break;
            case hpl::MaterialID::Water:
                break;
            default:
                break;
        }
        return std::monostate{};
    }
}
