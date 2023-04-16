$input v_texcoord0

#include <common.sh>

uniform vec4 u_param;
#define u_farPlane (u_param.x)

SAMPLER2D(s_diffuseMap, 0);
SAMPLER2D(s_depthMap, 1);

void main() {
    float vMul[9];
    float fOffset[9];
    vMul[0] = 0.05;
    vMul[1] = 0.1;
    vMul[2] = 0.25;
    vMul[3] = 0.3;
    vMul[4] = 0.1;
    vMul[5] = 0.3;
    vMul[6] = 0.25;
    vMul[7] = 0.1;
    vMul[8] = 0.05;
    fOffset[0] = -3.35;
    fOffset[1] = -2.35;
    fOffset[2] = -1.35;
    fOffset[3] = -0.35;
    fOffset[4] = 0.0;
    fOffset[5] = 0.35;
    fOffset[6] = 1.35;
    fOffset[7] = 2.35;
    fOffset[8] = 3.35;

    float fBlurSize = 2.0;
#if BLUR_HORIZONTAL == 1
    vec2 vOffsetMul = vec2(1.0, 0.0) * fBlurSize;
#else
    vec2 vOffsetMul = vec2(0.0, 1.0) * fBlurSize;
#endif

	//Get the core (at center) depth and create the minimum depth based on that.
	float fCoreDepth = texture2D(s_depthMap, gl_FragCoord.xy * u_viewTexel.xy).x;
	float fMinDepth =  fCoreDepth - 0.2 / u_farPlane;

    float vAmount = 0.0;
    float fMulSum = 0.0;

    for (int i = 0; i < 9; i += 1) {
        vec2 vCoordOffset = fOffset[i] * vOffsetMul;
        vec2 vUVPos = gl_FragCoord.xy + vCoordOffset;

        float vOcc = texture2D(s_diffuseMap, vUVPos * u_viewTexel.xy).r;
        float fDepth = texture2D(s_depthMap, vUVPos * u_viewTexel.xy).r;
        float fMul = vMul[i];

        // Skip any pixels where depth is lower (in front of) the core depth
        // Do not want foreground leaking into background (opposite is acceptable though)
        if (fDepth < fMinDepth) {
            fMul *= 0.25;
        }
 
        vOcc *= fMul;

        fMulSum += fMul;
        vAmount += vOcc;
    }
    vAmount /= fMulSum;
    gl_FragColor.xyz = vec3(vAmount,vAmount,vAmount);
    
}
