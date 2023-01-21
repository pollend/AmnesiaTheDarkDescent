$input v_clipPosition

#include <common.sh>

uniform vec4 u_param;
#define u_lightRadius (u_param.x)

uniform vec4 u_lightPos;
uniform vec4 u_lightColor;

uniform mat4 u_mtxInvViewRotation;

SAMPLERCUBE(s_goboMap, 0);

SAMPLER2D(s_diffuseMap, 1);
SAMPLER2D(s_normalMap, 2);
SAMPLER2D(s_positionMap, 3);
SAMPLER2D(s_specularMap, 4);
SAMPLER2D(s_attenuationLightMap, 5);

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

    vec3 goboVal = vec3(1.0);
#ifdef USE_GOBO_MAP
    vec4 worldLightDir = u_mtxInvViewRotation * vec4(lightDir,1.0);
    goboVal = textureCube(s_goboMap, worldLightDir.xyz).xyz;
#endif

	/////////////////////////////////
	//Calculate diffuse color
    float fLDotN = max(dot(normalizedNormal, normalLightDir), 0.0);
	vec3 diffuseColor = color.xyz * u_lightColor.xyz * fLDotN;


    vec3 specularColor = vec3(0.0);
    if(u_lightColor.w > 0.0) {
        vec3 halfVec = normalize(normalLightDir + normalize(-position));
        float specIntensity = specular.x;
        float specPower = specular.y;
        specularColor = vec3(u_lightColor.w * specIntensity *  pow( clamp( dot( halfVec, normalizedNormal), 0.0, 1.0), specPower )) * u_lightColor.xyz;
    }

    gl_FragColor.xyz = (specularColor + diffuseColor) * goboVal * attenuation;
}