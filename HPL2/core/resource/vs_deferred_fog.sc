$input a_position
$output v_position, v_ray

#include <common.sh>

uniform mat4 u_mtxInvRotation;

void main()
{
    vec3 wpos = mul(u_model[0], vec4(a_position, 1.0)).xyz;
    v_position = mul(u_view, vec4(wpos.xyz, 1.0)).xyz;

    #ifdef USE_OUTSIDE_BOX
        #ifdef USE_BACK_SIDE 
            v_ray = mul(u_mtxInvRotation, vec4(v_position, 1)).xyz;
        #endif
    #endif
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0));
}
