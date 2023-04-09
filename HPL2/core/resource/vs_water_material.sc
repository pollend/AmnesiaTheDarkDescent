$input a_position, a_texcoord0, a_tangent, a_normal
$output v_position, v_normal, v_tangent, v_bitangent, v_texcoord0

#include <common.sh>

void main()
{
    vec3 wpos = mul(u_model[0], vec4(a_position, 1.0)).xyz;
    mat3 normalMtx = normalMatrix3x3(mul(u_view, u_model[0]));
    
    v_position = mul(u_view, vec4(wpos.xyz, 1.0)).xyz;
    
    v_normal = normalize(mul(normalMtx, a_normal.xyz)).xyz;
    v_tangent = normalize(mul(normalMtx, a_tangent.xyz)).xyz;
    v_bitangent = normalize(mul(normalMtx, cross(a_normal, a_tangent))).xyz;
    v_texcoord0 = a_texcoord0;
    
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0));
    
}