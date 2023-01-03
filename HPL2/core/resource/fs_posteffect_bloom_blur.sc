
$input v_texcoord0

#include <common.sh>

SAMPLER2D(s_diffuseMap, 0);

uniform vec4 u_params;
#define u_useHorizontal (u_params.x)
#define u_blurSize  	(u_params.y)

const float fMulSum = 0.25+0.3+0.5+0.3+0.25;
void main()
{
    float vMul[9];
    float fOffset[9];
    vMul[0] = 0.25; vMul[1] = 0.3; vMul[2] = 0.5; vMul[3] = 0.3; vMul[4] = 0.25;
    fOffset[0] = -2.5; fOffset[1] = -0.75; fOffset[2] = 0.0; fOffset[3] = 0.75; fOffset[4] = 2.5;

	vec3 vAmount =vec3(0);
	
    vec2 vOffsetMul;
	if(0.0 < u_useHorizontal) {
		vec2 vOffsetMul = vec2(1.0, 0.0)*u_blurSize;
	} else {
		vec2 vOffsetMul = vec2(0.0, 1.0)*u_blurSize;
	}
	
	for(int i=0; i<5; i+=1)
	{	
		vec2 vOffset = vec2(fOffset[i])*vOffsetMul;
		vec3 vColor = texture2D(s_diffuseMap, v_texcoord0.xy + vOffset).xyz;
		vAmount += vColor * vMul[i];
	}
	
	vAmount /= fMulSum;
	
	gl_FragColor.xyz = vAmount;
	gl_FragColor.w = 1.0;
}