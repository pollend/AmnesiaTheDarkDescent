$input a_position
$output v_position, v_ray

#include <common.sh>

uniform mat4 u_boxInvViewModelRotation;

void main()
{
    vec3 wpos = mul(u_model[0], vec4(a_position, 1.0)).xyz;
    v_position = mul(u_view, vec4(wpos.xyz, 1.0)).xyz;
    v_ray = (u_boxInvViewModelRotation * vec4(v_position, 1)).xyz;

    gl_Position = mul(u_viewProj, vec4(wpos, 1.0));
}
