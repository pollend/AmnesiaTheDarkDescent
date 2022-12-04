$input v_texcoord0, v_color

#include <common.sh>

SAMPLER2D(s_diffuseMap, 0);

void main()
{
	vec4 vFinalColor = texture2D(s_diffuseMap, v_texcoord0);
	gl_FragColor = vFinalColor * v_color;
}