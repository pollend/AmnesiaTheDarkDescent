$input v_texcoord0

#include <common.sh>
SAMPLER2D(s_diffuseMap, 0);

void main()
{
	gl_FragColor = texture2D(s_diffuseMap, v_texcoord0.xy);
}