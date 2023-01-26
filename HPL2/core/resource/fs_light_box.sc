
#include <common.sh>

SAMPLER2D(s_diffuseMap, 0);
// SAMPLER2D(s_SSAOMap, 1);

uniform vec4 u_lightColor;

void main()
{
    vec2 ndc = gl_FragCoord.xy * u_viewTexel.xy;

	vec4 color =  texture2D(s_diffuseMap, ndc);
	
	//	color *= texture2D(aSSAOMap, ndc * 0.5);	//SSAO should be half the size of the screen.
	
	// Multiply with light color and AO (w).
	gl_FragColor.xyz = color.xyz * u_lightColor.xyz; 

}