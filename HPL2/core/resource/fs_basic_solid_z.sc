$input v_texcoord0

#include <common.sh>

uniform vec4 u_params;
#define u_useAlphaMap (u_params.x)
#define u_useDissolveFilter (u_params.y)

SAMPLER2D(diffuseMap, 0);
SAMPLER2D(dissolveMap, 1);

void main()
{
    vec4 vFinalColor;
    if(0.0 < u_useAlphaMap) {
        vFinalColor = texture2D(diffuseMap, v_texcoord0);
    } else {
        vFinalColor = vec4(1.0);
    }
    
    if(0.0 < u_useDissolveFilter) {
        vec2 vDissolveCoords = gl_FragCoord.xy * (1.0/128.0); //128 = size of dissolve texture.
        float fDissolve = texture2D(dissolveMap, vDissolveCoords).w;

        //Get in 0.5 - 1 range.
        fDissolve = fDissolve*0.5 + 0.5;
        vFinalColor.w = fDissolve - (1.0 - vFinalColor.w) * 0.5;
    }
		 
	gl_FragColor = vFinalColor;
}