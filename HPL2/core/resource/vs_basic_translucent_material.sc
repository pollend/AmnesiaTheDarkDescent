
$input a_position, a_texcoord0, a_tangent, a_normal, a_color0
$output v_color0, v_texcoord0, v_normal, v_tangent, v_bitangent, v_position

#include <common.sh>

uniform vec4 u_overrideColor;
uniform mat4 u_mtxUV;

void main()
{
    vec3 wpos = mul(u_model[0], vec4(a_position, 1.0)).xyz;
    mat4 normalMtx = normalMatrix(u_view * u_model[0]);
    v_position = mul(u_view, vec4(wpos.xyz, 1.0)).xyz;
    v_color0 = a_color0;
    
    v_normal = normalize(mat3(normalMtx) * a_normal.xyz);
    v_tangent = normalize(mat3(normalMtx) * a_tangent.xyz);
    v_bitangent = normalize(mat3(normalMtx) * cross(a_tangent.xyz, a_normal.xyz));
    v_texcoord0 = (u_mtxUV * vec4(a_texcoord0, 0, 1.0)).xy;

    gl_Position = mul(u_viewProj, vec4(wpos, 1.0));
}