$input v_texcoord0

#include <common.sh>

SAMPLER2D(s_positionMap, 0);
SAMPLER2D(s_normalMap, 1);
SAMPLER2D(s_scatterDisk, 2);

uniform vec4 u_param[2];
#define u_inputRTSize (u_param[0].xy)
#define u_negInvFarPlane (u_param[0].z)
#define u_farPlane (u_param[0].w)

#define u_depthDiffMul (u_param[1].x)
#define u_scatterDepthMul (u_param[1].y)
#define u_scatterLengthMin (u_param[1].z)
#define u_scatterLengthMax (u_param[1].w)

#define SSAONumDiv2 (8 / 2)
#define SSAO_BIAS (0.0025)

void main() {

    ///////////////
    // This is the core depth that we compare to
    float normalizedDepth = texture2D(s_positionMap, v_texcoord0.xy).z * u_negInvFarPlane;

    // Have a max limit on the length, or else there will be major slowdowns when many objects are upfront.
    // Multiply with height (y) since width varies with aspect!
    // Also added a min length to make stuff darker at a distance to avoid flickering.
    float fScatterLength =
        clamp(u_scatterDepthMul / (normalizedDepth * u_farPlane), u_scatterLengthMin, u_scatterLengthMax) * u_inputRTSize.y;
    float fScatterDiskZ = 0.0;

    
    vec2 vScreenScatterCoord = (gl_FragCoord.xy) * 1.0 / 4.0; // 4 = size of scatter texture, and this is to get a 1-1 pixel-texture usage.

    vScreenScatterCoord.y = fract(vScreenScatterCoord.y); // Make sure the coord is in 0 - 1 range
    vScreenScatterCoord.y *= 1.0 / SSAONumDiv2; // Access only first texture piece

    ///////////////////////////////////////////
    // Depth enhance
    float fOccSum = 0.0;
    float fFarPlaneMulDepthDiffMul = u_farPlane * u_depthDiffMul;

    //////////////////////////////////////////
    // Go through the samples, 4 at a time!
    for (int i = 0; i < (SSAONumDiv2 / 2); i++) {
        // Get the scatter coordinates (used to get the randomized postion for each sampling)
        vec2 vScatterLookupCoord1 = vec2(vScreenScatterCoord.x, vScreenScatterCoord.y + fScatterDiskZ * 4.0);

        vec4 vOffset1 = (texture2D(s_scatterDisk, vScatterLookupCoord1) * 2.0 - 1.0) * fScatterLength;

        vec2 s1 = v_texcoord0.xy + vOffset1.xy * u_viewTexel.xy;
        vec2 s2 = v_texcoord0.xy + vOffset1.zw * u_viewTexel.xy;
        vec2 s3 = v_texcoord0.xy - vOffset1.xy * u_viewTexel.xy;
        vec2 s4 = v_texcoord0.xy - vOffset1.zw * u_viewTexel.xy;

        // Look up the depth at the random samples. Notice that x-z and y-w are each others opposites! (important for extra polation
        // below!)
        vec4 vDepth = vec4(
            texture2D(s_positionMap, s1).z * u_negInvFarPlane,
            texture2D(s_positionMap, s2).z * u_negInvFarPlane,
            texture2D(s_positionMap, s3).z * u_negInvFarPlane,
            texture2D(s_positionMap, s4).z * u_negInvFarPlane);

        // The z difference in world coords multplied with DepthDiffMul
        vec4 vDiff = (normalizedDepth - vDepth) * fFarPlaneMulDepthDiffMul;
    
        // Invert the difference (so positive means uncovered) and then give unocvered values a slight advantage
        // Also set a max negative value (limits how much covered areas can affect.)
        vDiff = max(vec4(1) - vDiff, -0.7);

        // Caclculate the occulsion value, (the squaring makes sharper dark areas)
        vec4 vOcc = min(vDiff * vDiff, 1.0f);
        
        fOccSum += (normalizedDepth >= vDepth.x + SSAO_BIAS ?  1.0 : vOcc.x);
        fOccSum += (normalizedDepth >= vDepth.y + SSAO_BIAS ?  1.0 : vOcc.y);
        fOccSum += (normalizedDepth >= vDepth.z + SSAO_BIAS ?  1.0 : vOcc.z);
        fOccSum += (normalizedDepth >= vDepth.w + SSAO_BIAS ?  1.0 : vOcc.w);

        // Change the z coord for random coord look up (so new values are used on next iteration)
        fScatterDiskZ += (1.0 / SSAONumDiv2) * 2.0;
    }

    float fOcc = fOccSum / (2.0 * SSAONumDiv2);
    gl_FragColor.x = fOcc * fOcc;

}
