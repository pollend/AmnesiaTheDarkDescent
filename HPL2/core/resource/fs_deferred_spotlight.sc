$input v_clipPosition

#include <common.sh>

uniform vec4 u_param;
#define u_hasGobo (u_param.x)
#define u_hasSpecular (u_param.y)
#define u_lightRadius (u_param.z)

uniform vec4 u_lightPos;
uniform vec4 u_lightColor;

SAMPLER2D(s_diffuseMap, 0);
SAMPLER2D(s_normalMap, 1);
SAMPLER2D(s_positionMap, 2);
SAMPLER2D(s_specularMap, 4);

SAMPLER2D(s_attenuationLightMap, 5);

void main()
{
    vec2 ndc = gl_FragCoord.xy * u_viewTexel.xy;

    vec4 color =  texture2D(s_diffuseMap, ndc);
    vec4 normal = texture2D(s_normalMap, ndc);
    vec3 position =  texture2D(s_positionMap, ndc).xyz;

    vec3 lightDir = (u_lightPos.xyz -  position) * (1.0 / u_lightRadius);
    float attenuation = texture2D(s_attenuationLightMap, vec2(dot(lightDir, lightDir), 0.0)).x;
    
	/////////////////////////////////
	//Calculate diffuse color
    float fLDotN = max(dot(lightDir, normal.xyz), 0.0);
	vec3 diffuseColor = color.xyz * u_lightColor.xyz * fLDotN;

    gl_FragColor.xyz = diffuseColor * attenuation;
}