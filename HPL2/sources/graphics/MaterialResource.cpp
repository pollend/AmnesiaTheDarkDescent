#include <graphics/MaterialResource.h>
#include "graphics/Material.h"
#include "graphics/RendererDeferred.h"
#include "tinyimageformat_query.h"

namespace hpl::material {



    UnifomMaterialBlock resolveMaterialBlock(cMaterial& material, const MaterialBlockOptions& options) {
        UnifomMaterialBlock block = {};
        auto& descriptor = material.Descriptor();
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
        switch(descriptor.m_id) {
            case hpl::cMaterial::MaterialID::SolidDiffuse:
                block.m_solid.m_materialConfig = flags |
                    (descriptor.m_info.m_solid.m_alphaDissolveFilter ? UseDissolveFilter : 0);
                block.m_solid.m_heightMapScale = descriptor.m_info.m_solid.m_heightMapScale ;
                block.m_solid.m_heightMapBias = descriptor.m_info.m_solid.m_heightMapBias;
                block.m_solid.m_frenselBias = descriptor.m_info.m_solid.m_frenselBias ;
                block.m_solid.m_frenselPow = descriptor.m_info.m_solid.m_frenselPow;
                break;
            case hpl::cMaterial::MaterialID::Translucent:
                block.m_translucenct.m_materialConfig = flags |
                    (options.m_enableRefration && descriptor.m_info.m_translucent.m_hasRefraction ? UseRefractionNormals : 0) |
                    (options.m_enableReflection && descriptor.m_info.m_translucent.m_hasRefraction && descriptor.m_info.m_translucent.m_refractionEdgeCheck ? UseRefractionEdgeCheck : 0);
                block.m_translucenct.mfRefractionScale = descriptor.m_info.m_translucent.m_refractionScale;
                block.m_translucenct.mfFrenselBias = descriptor.m_info.m_translucent.m_frenselBias;
                block.m_translucenct.mfFrenselPow = descriptor.m_info.m_translucent.m_frenselPow;
                block.m_translucenct.mfRimLightMul = descriptor.m_info.m_translucent.m_rimLightMul;
                block.m_translucenct.mfRimLightPow = descriptor.m_info.m_translucent.m_rimLightPow;
            case hpl::cMaterial::MaterialID::Water:
                block.m_translucenct.m_materialConfig = flags;

                block.m_water.mfRefractionScale = descriptor.m_info.m_translucent.m_refractionScale;
 		        block.m_water.mfFrenselBias = descriptor.m_info.m_water.m_frenselBias;
		        block.m_water.mfFrenselPow = descriptor.m_info.m_water.m_frenselBias;

		        block.m_water.mfReflectionFadeStart = descriptor.m_info.m_water.mfReflectionFadeStart;
		        block.m_water.mfReflectionFadeEnd = descriptor.m_info.m_water.mfReflectionFadeEnd;
		        block.m_water.mfWaveSpeed = descriptor.m_info.m_water.mfWaveSpeed;
		        block.m_water.mfWaveAmplitude = descriptor.m_info.m_water.mfWaveAmplitude;

		        block.m_water.mfWaveFreq = descriptor.m_info.m_water.mfWaveFreq;

                break;
            default:
                break;
        }
        return block;
    }
    bool resolveRefraction(cMaterial& mat) {
        return true;
    }
    bool resolveReflection(cMaterial& mat) {
        return true;
    }
}
