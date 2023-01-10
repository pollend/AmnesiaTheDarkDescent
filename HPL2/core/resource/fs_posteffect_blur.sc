$input v_texcoord0

#include <common.sh>

uniform vec4 u_param;
#define u_useHorizontal (u_param.x)
#define u_blurSize	(u_param.y)
#define u_imageSize	(u_param.zw)

SAMPLER2D(s_diffuseMap, 1);

void main()
{
	float fMulSum = 0.25 + 0.3 + 0.5 + 0.3 + 0.25;
	float vMul[9];
	float fOffset[9];
	vMul[0] = 0.25; vMul[1] = 0.3; vMul[2] = 0.5; vMul[3] = 0.3; vMul[4] = 0.25;
	fOffset[0] = -2.5; fOffset[1] = -0.75; fOffset[2] = 0.0; fOffset[3] = 0.75; fOffset[4] = 2.5;

	vec3 vAmount =vec3(0);
	
    vec2 vOffsetMul;
	if(0.0 < u_useHorizontal) {
		vOffsetMul = vec2(1.0, 0.0)*u_blurSize;
	} else {
		vOffsetMul = vec2(0.0, 1.0)*u_blurSize;
	}

	for(int i=0; i < 5; i++)
	{
		vec2 vOffset = (vec2(fOffset[i]) / u_imageSize)*vOffsetMul;
		vec3 vColor = texture2D(s_diffuseMap, v_texcoord0.xy + vOffset).xyz;
		vAmount += vColor * vMul[i];
	}
	
	vAmount /= fMulSum;
	
	gl_FragColor = vec4(vAmount, 1.0);
}