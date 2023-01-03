$input v_texcoord0

#include <common.sh>

SAMPLER2D(s_diffuseMap, 0);
SAMPLER2D(s_blurMap, 1);

uniform vec4 u_rgbToIntensity;

void main()
{
	vec4 vBlurColor = 	texture2D(s_blurMap, 	  v_texcoord0.xy);
	vec4 vDiffuseColor = 	texture2D(s_diffuseMap, v_texcoord0.xy);
	vBlurColor *= vBlurColor * dot(vBlurColor.xyz, u_rgbToIntensity.xyz);
	
	gl_FragColor = vDiffuseColor + vBlurColor;
}