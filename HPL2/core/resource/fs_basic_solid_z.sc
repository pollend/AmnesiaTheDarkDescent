$input v_texcoord0

#include <common.sh>

uniform vec4 u_params;
#define u_useAlphaMap (u_params.x)
#define u_useDissolveFilter (u_params.y)
#define u_useDissolveAlphaMap (u_params.z)

SAMPLER2D(s_diffuseMap, 0);
SAMPLER2D(s_dissolveMap, 1);
SAMPLER2D(s_dissolveAlphaMap, 2);

void main()
{
    vec4 vFinalColor;
    if(0.0 < u_useAlphaMap) {
        vFinalColor = texture2D(s_diffuseMap, v_texcoord0);
    } else {
        vFinalColor = vec4(1.0);
    }
    
    if(0.0 < u_useDissolveFilter) {
        vec2 vDissolveCoords = v_texcoord0.xy * (1.0/128.0); //128 = size of dissolve texture.
        float fDissolve = texture2D(s_dissolveMap, vDissolveCoords).w;

        if(0.0 < u_useDissolveAlphaMap) {
            //Get in 0.75 - 1 range
			fDissolve = fDissolve*0.25 + 0.75;
			
			float fDissolveAlpha = texture2D(s_dissolveAlphaMap, v_texcoord0.xy).w;
			fDissolve -= (0.25 - fDissolveAlpha*0.25);
        } else {
			//Get in 0.5 - 1 range.
			fDissolve = fDissolve*0.5 + 0.5;
        }

        //Get in 0.5 - 1 range.
        fDissolve = fDissolve*0.5 + 0.5;
        vFinalColor.w = fDissolve - (1.0 - vFinalColor.w) * 0.5;
    }
		 
	gl_FragColor = vFinalColor;
}