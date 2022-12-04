
$input a_position, a_texcoord0, a_tangent, a_normal
$output v_texcoord0, v_normal, v_tangent, v_bitangent, v_position, v_view

#include <common.sh>

uniform mat4 u_normalMtx;
uniform mat4 u_mtxUV;

uniform vec4 u_params[1];
#define u_useNormalMap (u_params[0].x)
#define u_useSpecular (u_params[0].y)
#define u_useEnvMap (u_params[0].z)
#define u_useCubeMapAlpha (u_params[0].w)

#define u_useParallax (u_params[1].x)

void main()
{
    vec3 wpos = mul(u_model[0], vec4(a_position, 1.0)).xyz;
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0));
    v_position = gl_Position.xyz ;

    if (0.0 < u_useNormalMap) {

        v_tangent = normalize(u_normalMtx * vec4(a_tangent.xyz, 1.0)).xyz;
        v_bitangent = normalize(u_normalMtx * vec4(cross(a_normal, a_tangent), 1.0)).xyz;

        vec3 viewEye =  (u_view *  vec4(wpos, 1.0)).xyz;
        v_view = vec3(
            dot(viewEye, v_tangent), 
            dot(viewEye, v_bitangent), 
            dot(-viewEye, a_normal));
    } else {
        v_normal = normalize(mul(u_normalMtx, vec4(a_normal.xyz, 1.0))).xyz;
    }
    
    v_texcoord0 = (u_mtxUV * vec4(a_texcoord0, 0, 0)).xy;
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    
}