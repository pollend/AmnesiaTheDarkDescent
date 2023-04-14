$input v_normal, v_texcoord0

#include <common.sh>

SAMPLER2D(s_diffuseMap, 0);

uniform vec4 u_param;
#define u_colorMul (u_param.x)

void main()
{
	float rightLight = dot(v_normal, vec3(0.0, 0.0, -1.0));
	rightLight = 1.0 - abs(rightLight);
	vec4 color = texture2D(s_diffuseMap, v_texcoord0);
	
	if(color.w < 0.5)
		discard;

	gl_FragColor = color * vec4(0.5, 0.5, 1.0, 0.0) * rightLight * u_colorMul;
}