$input v_texcoord0

#include <common.sh>

SAMPLER2D(s_positionMap, 0);

uniform vec4 u_fogColor;
uniform vec4 u_params;

#define u_fogStart u_params.x
#define u_fogLength u_params.y
#define u_fogFalloffExp u_params.z

void main()
{
    float fDepth = texture2D(s_positionMap, v_texcoord0).z;
    fDepth = min(fDepth - u_fogStart, u_fogLength);
	float fAmount = max(fDepth / u_fogLength, 0.0);
	
    gl_FragColor.xyz = u_fogColor.xyz;
    gl_FragColor.w = pow(fAmount, u_fogFalloffExp) * u_fogColor.w;
}
