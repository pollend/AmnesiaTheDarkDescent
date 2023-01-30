$input v_texcoord0

#include <common.sh>

SAMPLER2D(s_diffuseMap, 0);

uniform vec4 u_param;
#define u_colorMul (u_param.x)

void main()
{
	gl_FragColor = mul(texture2D(s_diffuseMap, v_texcoord0.xy), u_colorMul);
}