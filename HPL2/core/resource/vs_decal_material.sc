
$input a_position, a_texcoord0, a_color0
$output v_texcoord0, v_color0

#include <common.sh>

uniform mat4 u_mtxUV;

void main()
{
    vec3 wpos = mul(u_model[0], vec4(a_position, 1.0)).xyz;
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0));
	v_texcoord0 = (u_mtxUV * vec4(a_texcoord0, 0, 1.0)).xy;
	v_color0 = a_color0;
}