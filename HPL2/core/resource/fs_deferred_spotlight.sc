$input v_clipPosition

#include <common.sh>

#define SHADOW_MAP_BIAS  0.01

uniform vec4 u_param[2];
#define u_lightRadius (u_param[0].x)
#define u_lightForward (u_param[0].yzw)

#define u_oneMinusCosHalfSpotFov (u_param[1].x)
#define u_shadowMapOffsetMul (u_param[1].yz)

#define SHADOW_JITTER_SAMPLES_DIV_2 (SHADOW_JITTER_SAMPLES/2)

uniform vec4 u_lightPos;
uniform vec4 u_lightColor;
uniform mat4 u_spotViewProj;

SAMPLER2D(s_diffuseMap, 0);
SAMPLER2D(s_normalMap, 1);
SAMPLER2D(s_positionMap, 2);
SAMPLER2D(s_specularMap, 3);
SAMPLER2D(s_attenuationLightMap, 4);
#ifdef USE_GOBO_MAP
    SAMPLER2D(s_goboMap, 5);
#else
    SAMPLER2D(s_spotFalloffMap, 5);
#endif

SAMPLER2DSHADOW(s_shadowMap, 6);
SAMPLER2D(s_shadowOffsetMap, 7);

void main()
{
    vec2 ndc = gl_FragCoord.xy * u_viewTexel.xy;

    vec4 color =  texture2D(s_diffuseMap, ndc);
    vec4 normal = texture2D(s_normalMap, ndc);
    vec3 position =  texture2D(s_positionMap, ndc).xyz;
    vec2 specular =  texture2D(s_specularMap, ndc).xy;

    vec3 lightDir = (u_lightPos.xyz -  position) * (1.0 / u_lightRadius);
    float attenuation = texture2D(s_attenuationLightMap, vec2(dot(lightDir, lightDir), 0.0)).x;

    vec3 normalLightDir = normalize(lightDir);
    vec3 normalizedNormal = normalize(normal.xyz);

    vec4 projectionUV = u_spotViewProj * vec4(position,1.0);
        
    vec3 goboVal = vec3(1.0);
#ifdef USE_GOBO_MAP
    goboVal = texture2DProj(s_goboMap, projectionUV).xyz;
#else 
    float fOneMinusCos = 1.0 - dot( normalLightDir,  u_lightForward);
    attenuation *= texture2D(s_spotFalloffMap, vec2(fOneMinusCos / u_oneMinusCosHalfSpotFov, 0.0)).x;
#endif
    
	/////////////////////////////////
	//Calculate diffuse color
    float fLDotN = max(dot(normalizedNormal, normalLightDir), 0.0);
	vec3 diffuseColor = color.xyz * u_lightColor.xyz * fLDotN;
    float bias = max(SHADOW_MAP_BIAS * (1.0 - dot(normalizedNormal, normalLightDir)), SHADOW_MAP_BIAS);

    #ifdef USE_SHADOWS
        #ifdef SHADOW_JITTER_SIZE
            float fShadowSum = 0.0;
            float fJitterZ = 0.0;
            vec2 vScreenJitterCoord = gl_FragCoord.xy * (1.0 / float(SHADOW_JITTER_SIZE));

            vScreenJitterCoord.y = fract(vScreenJitterCoord.y);	 //Make sure the coord is in 0 - 1 range
            vScreenJitterCoord.y *= 1.0 / float(SHADOW_JITTER_SAMPLES_DIV_2);	 //Access only first texture piece
            
            for(int i=0; i< SHADOW_JITTER_SAMPLES_DIV_2; i++)
            {
                vec2 jitterLookupCoord = vec2(vScreenJitterCoord.x, vScreenJitterCoord.y + fJitterZ);
                
                vec4 offset = texture2D(s_shadowOffsetMap, jitterLookupCoord) *10.0 - 5.0;
                fShadowSum += shadow2DProj(s_shadowMap, vec4(projectionUV.xy + (vec2(offset.xy) * u_shadowMapOffsetMul), projectionUV.z - bias, projectionUV.w) );
                fShadowSum += shadow2DProj(s_shadowMap, vec4(projectionUV.xy + (vec2(offset.zw) * u_shadowMapOffsetMul), projectionUV.z - bias, projectionUV.w) );
                            
                fJitterZ += 1.0 / float(SHADOW_JITTER_SAMPLES_DIV_2);
            }
            
            fShadowSum /= float(SHADOW_JITTER_SAMPLES);
            attenuation *= fShadowSum;
        #else
            attenuation *= shadow2DProj(s_shadowMap, vec4(projectionUV.xy, projectionUV.z - bias, projectionUV.w));
        #endif
    #endif

    vec3 specularColor = vec3(0.0);
    if(u_lightColor.w > 0.0) {
        vec3 halfVec = normalize(normalLightDir + normalize(-position));
        float specIntensity = specular.x;
        float specPower = specular.y;
        specularColor = vec3(u_lightColor.w * specIntensity *  pow( clamp( dot( halfVec, normalizedNormal), 0.0, 1.0), specPower )) * u_lightColor.xyz;
    }

    gl_FragColor.xyz = (specularColor + diffuseColor) * goboVal * attenuation;
}