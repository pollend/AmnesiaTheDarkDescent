

$input a_position, a_texcoord0
$output v_texcoord0

#include <common.sh>

uniform mat4 u_mtxUV;

void main()
{
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0) );
    v_texcoord0 = (u_mtxUV * vec4(a_texcoord0, 0, 0)).xy;
}