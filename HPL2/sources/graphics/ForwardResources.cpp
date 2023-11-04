#include "graphics/ForwardResources.h"
#include "graphics/GraphicsTypes.h"
#include "graphics/Material.h"

#include <cstdint>
#include <variant>

namespace hpl::resource  {

    LightResourceVariants CreateFromLight(iLight& light) {
        return std::monostate{};
    }

    void visitTextures(MaterialTypes& material, std::function<void(eMaterialTexture, uint32_t slot)> handler) {
        if(auto* item = std::get_if<DiffuseMaterial>(&material)) {
            struct {
                eMaterialTexture m_type;
                uint16_t* m_value;
            } m_textures[] = {
                {eMaterialTexture_Diffuse, &item->m_tex.m_diffues},
                {eMaterialTexture_NMap, &item->m_tex.m_normal},
                {eMaterialTexture_Specular, &item->m_tex.m_specular},
                {eMaterialTexture_Alpha, &item->m_tex.m_alpha},
                {eMaterialTexture_Height, &item->m_tex.m_height},
                {eMaterialTexture_Illumination, &item->m_tex.m_illuminiation},
                {eMaterialTexture_DissolveAlpha, &item->m_tex.m_dissolveAlpha},
                {eMaterialTexture_CubeMapAlpha, &item->m_tex.m_cubeMapAlpha},
            };
            for(auto& tex: m_textures) {
                if(*tex.m_value != UINT16_MAX) {
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
                    uint16_t* m_value;
                } m_textures[] = {
                    {eMaterialTexture_Diffuse, &result.m_tex.m_diffues},
                    {eMaterialTexture_NMap, &result.m_tex.m_normal},
                    {eMaterialTexture_Alpha, &result.m_tex.m_alpha},
                    {eMaterialTexture_Specular, &result.m_tex.m_specular},
                    {eMaterialTexture_Height, &result.m_tex.m_height},
                    {eMaterialTexture_Illumination, &result.m_tex.m_illuminiation},
                    {eMaterialTexture_DissolveAlpha, &result.m_tex.m_dissolveAlpha},
                    {eMaterialTexture_CubeMapAlpha, &result.m_tex.m_cubeMapAlpha},
                };
                for(auto& tex: m_textures) {
                    auto* image = material->GetImage(tex.m_type);
                    (*tex.m_value) = UINT16_MAX;
                    if(image) {
                        uint32_t handle = pool.request(image->GetTexture());
                        (*tex.m_value) = static_cast<uint16_t>(handle);
                    }
                }
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
