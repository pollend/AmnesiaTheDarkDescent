#include <graphics/MaterialResource.h>
#include "graphics/Enum.h"
#include "graphics/Material.h"
#include "graphics/RendererDeferred.h"
#include "tinyimageformat_query.h"

namespace hpl::material {
    uint32_t resolveMaterialConfig(cMaterial& material, const MaterialBlockOptions& options) {
		const auto alphaMapImage = material.GetImage(eMaterialTexture_Alpha);
		const auto heightMapImage = material.GetImage(eMaterialTexture_Height);
        uint32_t flags =
			(material.GetImage(eMaterialTexture_Diffuse) ? EnableDiffuse: 0) |
			(material.GetImage(eMaterialTexture_NMap) ? EnableNormal: 0) |
 			(material.GetImage(eMaterialTexture_Specular) ? EnableSpecular: 0) |
			(alphaMapImage ? EnableAlpha: 0) |
			(heightMapImage ? EnableHeight: 0) |
			(material.GetImage(eMaterialTexture_Illumination) ? EnableIllumination: 0) |
			(material.GetImage(eMaterialTexture_CubeMap) ? EnableCubeMap: 0) |
			(material.GetImage(eMaterialTexture_DissolveAlpha) ? EnableDissolveAlpha: 0) |
			(material.GetImage(eMaterialTexture_CubeMapAlpha) ? EnableCubeMapAlpha: 0) |
			((alphaMapImage && TinyImageFormat_ChannelCount(static_cast<TinyImageFormat>(alphaMapImage->GetTexture().m_handle->mFormat)) == 1) ? IsAlphaSingleChannel: 0) |
			((heightMapImage && TinyImageFormat_ChannelCount(static_cast<TinyImageFormat>(heightMapImage->GetTexture().m_handle->mFormat)) == 1) ? IsHeightMapSingleChannel: 0);
        auto& descriptor = material.Type();
        switch(descriptor.m_id) {
            case hpl::MaterialID::SolidDiffuse:
                flags |=
                    (descriptor.m_solid.m_alphaDissolveFilter ? UseDissolveFilter : 0);
                break;
            case hpl::MaterialID::Translucent:
                flags |=
                    (options.m_enableRefration && descriptor.m_translucent.m_hasRefraction ? UseRefractionNormals : 0) |
                    (options.m_enableReflection && descriptor.m_translucent.m_hasRefraction && descriptor.m_translucent.m_refractionEdgeCheck ? UseRefractionEdgeCheck : 0);
                break;
            default:
                break;
        }
        return flags;
    }
    UnifomMaterialBlock resolveMaterialBlock(cMaterial& material, const MaterialBlockOptions& options) {
        UnifomMaterialBlock block = {};
        auto& descriptor = material.Type();
		const auto alphaMapImage = material.GetImage(eMaterialTexture_Alpha);
		const auto heightMapImage = material.GetImage(eMaterialTexture_Height);
        uint32_t flags = resolveMaterialConfig(material, options);
        switch(descriptor.m_id) {
            case hpl::MaterialID::SolidDiffuse:
                block.m_solid.m_materialConfig = flags;
                block.m_solid.m_heightMapScale = descriptor.m_solid.m_heightMapScale;
                block.m_solid.m_heightMapBias = descriptor.m_solid.m_heightMapBias;
                block.m_solid.m_frenselBias = descriptor.m_solid.m_frenselBias;
                block.m_solid.m_frenselPow = descriptor.m_solid.m_frenselPow;
                break;
            case hpl::MaterialID::Decal:
                block.m_decal.m_materialConfig = flags;
                break;
            case hpl::MaterialID::Translucent:
                block.m_translucenct.m_materialConfig = flags;
                block.m_translucenct.m_refractionScale = descriptor.m_translucent.m_refractionScale;
                block.m_translucenct.m_frenselBias = descriptor.m_translucent.m_frenselBias;
                block.m_translucenct.m_frenselPow = descriptor.m_translucent.m_frenselPow;
                block.m_translucenct.mfRimLightMul = descriptor.m_translucent.m_rimLightMul;
                block.m_translucenct.mfRimLightPow = descriptor.m_translucent.m_rimLightPow;
            case hpl::MaterialID::Water:
                block.m_water.m_materialConfig = flags;

                block.m_water.mfRefractionScale = descriptor.m_translucent.m_refractionScale;
 		        block.m_water.mfFrenselBias = descriptor.m_water.m_frenselBias;
		        block.m_water.mfFrenselPow = descriptor.m_water.m_frenselBias;

		        block.m_water.m_reflectionFadeStart = descriptor.m_water.mfReflectionFadeStart;
		        block.m_water.m_reflectionFadeEnd = descriptor.m_water.mfReflectionFadeEnd;
		        block.m_water.m_waveSpeed = descriptor.m_water.mfWaveSpeed;
		        block.m_water.mfWaveAmplitude = descriptor.m_water.mfWaveAmplitude;

		        block.m_water.mfWaveFreq = descriptor.m_water.mfWaveFreq;

                break;
            default:
                ASSERT(false && "unhandeled material config");
                break;
        }
        return block;
    }
}
