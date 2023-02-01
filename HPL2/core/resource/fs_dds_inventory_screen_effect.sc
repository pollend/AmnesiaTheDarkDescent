$input v_texcoord0

#include <common.sh>

SAMPLER2D(s_diffuseMap, 0);

void main()
{
	vec3 color = texture2D(s_diffuseMap, v_texcoord0).xyz;
	
	vec3 colorToR = vec3(0.39,  0.769, 0.189);
	vec3 colorToG = vec3(0.349, 0.686, 0.168);
	vec3 colorToB = vec3(0.272, 0.534, 0.131);
	
	float fIntensity = 0.35;
	
	vec3 vFinalCol;
	vFinalCol.x = dot(color, colorToR) * fIntensity;
	vFinalCol.y = dot(color, colorToG) * fIntensity;
	vFinalCol.z = dot(color, colorToB) * fIntensity;
	
	gl_FragColor.xyz = pow(vFinalCol.xyz, vec3(1.5, 1.5, 1.5));
	gl_FragColor.w = 1.0;
}