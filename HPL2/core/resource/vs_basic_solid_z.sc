$input a_position, a_texcoord0
$output v_texcoord0

#include <common.sh>

uniform mat4 u_mtxUV;

void main()
{
    v_texcoord0 = mul(u_mtxUV, vec4(a_texcoord0, 0, 0)).xy;

    vec3 wpos = mul(u_model[0], vec4(a_position, 1.0)).xyz;
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0));
}