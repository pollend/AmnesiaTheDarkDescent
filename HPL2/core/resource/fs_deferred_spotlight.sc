$input v_clipPosition

#include <common.sh>

uniform vec4 u_param[2];
#define u_useShadow (u_param[0].x)
#define u_hasSpecular (u_param[0].y)
#define u_lightRadius (u_param[0].z)

#define u_lightForward (u_param[1].xyz)
#define u_oneMinusCosHalfSpotFov (u_param[1].w)

uniform vec4 u_lightPos;
uniform vec4 u_lightColor;
uniform mat4 u_spotViewProj;

SAMPLER2D(s_diffuseMap, 0);
SAMPLER2D(s_normalMap, 1);
SAMPLER2D(s_positionMap, 2);
SAMPLER2D(s_specularMap, 3);

SAMPLER2D(s_attenuationLightMap, 4);
SAMPLER2D(s_spotFalloffMap, 5);

SAMPLER2DSHADOW(s_shadowMap, 6);

#define SHADOW_MAP_BIAS  0.01
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

    float fOneMinusCos = 1.0 - dot( normalLightDir,  u_lightForward);
    attenuation *= texture2D(s_spotFalloffMap, vec2(fOneMinusCos / u_oneMinusCosHalfSpotFov, 0.0)).x;

	/////////////////////////////////
	//Calculate diffuse color
    float fLDotN = max(dot(normalizedNormal, normalLightDir), 0.0);
	vec3 diffuseColor = color.xyz * u_lightColor.xyz * fLDotN;

    if (0.0 < u_useShadow) {
        vec4 projectionUV = u_spotViewProj * vec4(position,1.0);
        float bias = max(SHADOW_MAP_BIAS * (1.0 - dot(normalizedNormal, normalLightDir)), SHADOW_MAP_BIAS);
        attenuation *= shadow2DProj(s_shadowMap, vec4(projectionUV.xy, projectionUV.z - bias, projectionUV.w));
    }

    vec3 specularColor = vec3(0.0);
    if(u_lightColor.w > 0.0) {
        vec3 halfVec = normalize(normalLightDir + normalize(-position));
        float specIntensity = specular.x;
        float specPower = specular.y;
        specularColor = vec3(u_lightColor.w * specIntensity *  pow( clamp( dot( halfVec, normalizedNormal), 0.0, 1.0), specPower )) * u_lightColor.xyz;
    }

    gl_FragColor.xyz = (specularColor + diffuseColor) * attenuation;
}