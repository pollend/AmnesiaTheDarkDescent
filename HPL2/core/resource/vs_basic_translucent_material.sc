
$input a_position, a_texcoord0, a_tangent, a_normal, a_color0
$output v_texcoord0, v_normal, v_tangent, v_bitangent, v_position, v_color0

#include <common.sh>

uniform mat4 u_mtxUV;

void main()
{
    vec3 wpos = mul(u_model[0], vec4(a_position, 1.0)).xyz;
    mat4 normalMtx = normalMatrix(u_view * u_model[0]);
    
    v_position = mul(u_view, vec4(wpos.xyz, 1.0)).xyz;
    v_color0 = a_color0;
    v_normal = a_normal;
    v_tangent = normalize(normalMtx * vec4(a_tangent.xyz, 1.0)).xyz;
    v_bitangent = normalize(normalMtx * vec4(cross(a_normal, a_tangent), 1.0)).xyz;
    v_texcoord0 = (u_mtxUV * vec4(a_texcoord0, 0, 0)).xy;
    
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0));
}