
$input a_position, a_texcoord0, a_tangent, a_normal
$output v_texcoord0, v_normal, v_tangent, v_bitangent, v_position, v_view

#include <common.sh>

uniform mat4 u_mtxUV;

void main()
{
    vec3 wpos = mul(u_model[0], vec4(a_position, 1.0)).xyz;
    mat4 modelView = u_view * u_model[0];
    mat4 normalMtx = normalMatrix(u_view * u_model[0]);

    v_position = mul(u_view, vec4(wpos.xyz, 1.0)).xyz;

    v_normal = normalize(normalMtx * vec4(a_normal, 1.0)).xyz;
    v_tangent = normalize(normalMtx * vec4(a_tangent.xyz, 1.0)).xyz;
    v_bitangent = normalize(normalMtx * vec4(cross(a_normal, a_tangent), 1.0)).xyz;
    v_texcoord0 = (u_mtxUV * vec4(a_texcoord0, 0, 1.0)).xy;

    vec3 viewEye =  (modelView *  vec4(a_position, 1.0)).xyz;
    v_view = vec3(
        dot(viewEye, v_tangent), 
        dot(viewEye, v_bitangent), 
        dot(-viewEye, a_normal));
    
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0));
}