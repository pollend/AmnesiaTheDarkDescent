$input a_texcoord0, a_color0, a_position
$output v_texcoord0, v_color0, v_position

#include <common.sh>

void main()
{
    v_position = a_position;
    v_color = a_color0;
    v_texcoord0 = a_texcoord0;

    vec3 wpos = mul(u_model[0], vec4(a_position, 1.0)).xyz;
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0));
}