
$input v_texcoord0

#include <common.sh>

SAMPLER2D(s_diffuseMap, 0);
SAMPLER2D(s_ampMap0, 1);
SAMPLER2D(s_ampMap1, 2);
SAMPLER2D(s_zoomMap, 3);

uniform vec4 u_param[2];
#define u_alpha        (u_param[0].x)
#define u_fT           (u_param[0].y)
#define u_ampT         (u_param[0].z)
#define u_waveAlpha    (u_param[0].w)
#define u_zoomAlpha    (u_param[1].x)

void main()
{
	vec3 vAmp = texture2D(s_ampMap0, v_texcoord0).xyz*(1.0-u_ampT) + texture2D(s_ampMap1, v_texcoord0).xyz*u_ampT;
	vAmp *= u_waveAlpha * 0.04 * v_texcoord0.y;
	vec3 vZoom = texture2D(s_zoomMap, v_texcoord0).xyz;
	
	vec2 vUV = v_texcoord0;
	vUV += (vZoom.xy-vec2(0.5))*2.0* 0.6 * vZoom.z * u_zoomAlpha;
	
	vec2 vSinUv = vUV * 0.6;
	vUV.x += sin(u_fT + vSinUv.y) * vAmp.x;
	vUV.y += sin(u_fT + vSinUv.x*1.83) * vAmp.y;
	
	vec3 vDiffuseColor = texture2D(s_diffuseMap, vUV).xyz;
	gl_FragColor.xyz = vDiffuseColor;
}