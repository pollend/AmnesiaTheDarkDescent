
$input a_position, a_texcoord0, a_color
$output v_texcoord0, v_color

#include <common.sh>

uniform mat4 u_mtxUV;

void main()
{
    vec3 wpos = mul(u_model[0], vec4(a_position, 1.0)).xyz;
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0));
	v_texcoord0 = u_mtxUV * a_texcoord0;
	v_color = a_color;
}