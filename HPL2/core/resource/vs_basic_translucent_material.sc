$input a_position, a_texcoord0, a_tangent, a_normal, a_color0
$output v_position, v_color, v_normal, v_tangent, v_bitangent, v_texcoord0

#include <common.sh>

uniform vec4 u_overrideColor;
uniform mat4 u_mtxUV;

void main()
{
    vec3 wpos = mul(u_model[0], vec4(a_position, 1.0)).xyz;
    mat3 normalMtx = normalMatrix3x3(mul(u_view, u_model[0]));

    v_position = mul(u_view, vec4(wpos.xyz, 1.0)).xyz;
    v_color = a_color0;
    
    v_normal = normalize(mul(normalMtx, a_normal.xyz));
    v_tangent = normalize(mul(normalMtx, a_tangent.xyz));
    v_bitangent = normalize(mul(normalMtx, cross(a_tangent.xyz, a_normal.xyz)));
    v_texcoord0 = mul(u_mtxUV, vec4(a_texcoord0, 0, 1.0)).xy;
    
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0));
}