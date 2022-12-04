$input v_texcoord0

#include <common.sh>

SAMPLER2D(s_diffuseMap, 0);

uniform vec4 u_params;
#define u_colorMul (u_params.x)

void main()
{
	gl_FragColor = texture2D(s_diffuseMap, v_texcoord0.xy) * u_colorMul;
}