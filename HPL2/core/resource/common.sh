#include <bgfx_shader.sh>

float Fresnel(float afEDotN, float afFresnelBias, float afFresnelPow)
{
    float fFacing = 1.0 - afEDotN;
    return max(afFresnelBias+ (1.0-afFresnelBias)* pow(fFacing,afFresnelPow), 0.0); 
}
