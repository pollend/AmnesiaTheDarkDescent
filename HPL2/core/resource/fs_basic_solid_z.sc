$input v_texcoord0

#include <common.sh>

SAMPLER2D(s_diffuseMap, 0);
SAMPLER2D(s_dissolveMap, 1);
SAMPLER2D(s_dissolveAlphaMap, 2);


void main()
{
    vec4 vFinalColor;
    #ifdef USE_ALPHA_MAP
        vFinalColor = texture2D(s_diffuseMap, v_texcoord0);
    #else
        vFinalColor = vec4(1.0);
    #endif

    
    #ifdef USE_DISSOLVE_FILTER 
        vec2 vDissolveCoords = v_texcoord0.xy * (1.0/128.0); //128 = size of dissolve texture.
        float fDissolve = texture2D(s_dissolveMap, vDissolveCoords).w;

        #ifdef USE_DISSOLVE_ALPHA_MAP
            //Get in 0.75 - 1 range
			fDissolve = fDissolve*0.25 + 0.75;
			
			float fDissolveAlpha = texture2D(s_dissolveAlphaMap, v_texcoord0.xy).w;
			fDissolve -= (0.25 - fDissolveAlpha*0.25);
        #else
			//Get in 0.5 - 1 range.
			fDissolve = fDissolve*0.5 + 0.5;
        #endif

        //Get in 0.5 - 1 range.
        fDissolve = fDissolve*0.5 + 0.5;
        vFinalColor.w = fDissolve - (1.0 - vFinalColor.w) * 0.5;
    #endif
		 
	gl_FragColor = vFinalColor;
}