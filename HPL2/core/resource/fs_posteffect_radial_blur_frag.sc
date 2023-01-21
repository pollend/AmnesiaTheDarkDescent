#include <common.sh>

SAMPLER2D(s_diffuseMap, 0);

uniform vec4 u_params;
#define afSize  u_params.x
#define afBlurStartDist  u_params.y
#define avHalfScreenSize  (u_params.zw)

float fTotalMul = 1.1;
void main()
{
	float vColorMul[5]; 
	vColorMul[0] = 0.1;
	vColorMul[1] = 0.2;
	vColorMul[2] = 0.5;
	vColorMul[3] = 0.2;
	vColorMul[4] = 0.1;


	float vSizeMul[5]; 
	vSizeMul[0] = -1.0;
	vSizeMul[1] = -0.5;
	vSizeMul[2] = 0.0;
	vSizeMul[3] = 0.5;
	vSizeMul[4] = 1.0;

	vec2 vScreenCoord = gl_FragCoord.xy;
	
	vec2 vDir = avHalfScreenSize - vScreenCoord;
	float fDist = length(vDir) / avHalfScreenSize.x;
	vDir = normalize(vDir);
	
	fDist = max(0.0, fDist-afBlurStartDist);
	
	vDir *= fDist * afSize;
			
	vec3 vDiffuseColor = vec3(0.0);
	
	for(int i=0; i<5; ++i)
	{
		vDiffuseColor += texture2D(s_diffuseMap, vScreenCoord + vDir * vSizeMul[i]).xyz * vColorMul[i];
	}
	
	vDiffuseColor /= fTotalMul;
	
	gl_FragColor.xyz = vDiffuseColor;
	gl_FragColor.w = 1.0;
}