$input v_position, v_color, v_normal, v_tangent, v_bitangent, v_texcoord0

#include <common.sh>

SAMPLERCUBE(s_envMap, 0);
SAMPLER2D(s_diffuseMap, 1);
SAMPLER2D(s_normalMap, 2);
SAMPLER2D(s_refractionMap, 3);
SAMPLER2D(s_envMapAlphaMap, 4);

uniform mat4 u_mtxInvViewRotation;
uniform vec4 u_fogColor;

uniform vec4 u_param[4];
#define u_useCubeMapAlpha (u_param[0].x)
#define u_useScreenNormal (u_param[0].y)
#define u_fogStart (u_param[0].z)
#define u_fogLength (u_param[0].w)

#define u_alpha (u_param[1].x)
#define u_lightLevel (u_param[1].y)
#define u_oneMinusFogAlpha (u_param[1].z)
#define u_falloffExp (u_param[1].w)

#define u_refractionScale (u_param[2].x)

#define u_frenselBiasPow (u_param[3].xy)
#define u_rimLightMulPow (u_param[3].zw)

void main()
{
    vec4 vFinalColor = vec4(0.0, 0.0 ,0.0, 1.0);
 
    #ifdef USE_DIFFUSE_MAP  
        vFinalColor = texture2D(s_diffuseMap, v_texcoord0.xy) * v_color;
    #endif

    ////////////////////
    //Fog

    float fFinalAlpha = u_alpha;
    #ifdef USE_USE_FOG
        float fFogAmount = (-v_position.z - u_fogStart) / u_fogLength;
        fFogAmount = clamp(fFogAmount, 0.0, 1.0);
        fFogAmount = pow(fFogAmount, u_falloffExp);
        
        fFinalAlpha = u_oneMinusFogAlpha * fFogAmount + (1.0 - fFogAmount);
        fFinalAlpha *= u_alpha;
    #endif
    
    
    ////////////////////
    //Calculate new color based on Alpha and Blend mode
    #ifdef USE_BLEND_MODE_ADD
        vFinalColor.xyz *= fFinalAlpha*u_lightLevel;
    #endif
    #ifdef USE_BLEND_MODE_MUL
        vFinalColor.xyz += (vec3(1.0,1.0, 1.0) - vFinalColor.xyz) * (1.0-fFinalAlpha);
    #endif
    #ifdef USE_BLEND_MODE_MULX2
        float fBlendMulAlpha = u_lightLevel*fFinalAlpha;
        vFinalColor.xyz = mul(vFinalColor.xyz, fBlendMulAlpha) + mul(vec3(0.5,0.5,0.5), (1.0 - fBlendMulAlpha));
    #endif
    #ifdef USE_BLEND_MODE_ALPHA
        vFinalColor.xyz *= u_lightLevel;
    #endif
    #ifdef USE_BLEND_MODE_PREMUL_ALPHA
       vFinalColor *= fFinalAlpha;
        vFinalColor.xyz *= u_lightLevel;
    #endif

    
    vec3 mapNormal = vec3(0,0,0);
    vec3 screenNormal = vec3(0,0,0);
    #if defined(USE_REFRACTION) || defined(USE_CUBE_MAP)
        #ifdef USE_NORMAL_MAP
            mapNormal = texture2D(s_normalMap, v_texcoord0.xy).xyz*2.0 - 1.0; 	
            screenNormal = normalize(mapNormal.x * v_tangent + mapNormal.y * v_bitangent + mapNormal.z * v_normal);
        #else
            screenNormal = normalize(v_normal);
        #endif
    #endif

    ////////////////////
    //Refraction
    #ifdef USE_REFRACTION
        float invDist = min(1.0/v_position.z, 10.0);
            
        ///////////////////////
        // Sample refaraction map (using distorted coords)
        vec2 vDistortedScreenPos = gl_FragCoord.xy * u_viewTexel.xy;
        #ifdef USE_NORMAL_MAP
            //Should the screen normal or texture normal be used?
            vec2 vRefractOffset;
            if(0.0 < u_useScreenNormal) {
                vRefractOffset = screenNormal.xy;
            } else {
                vRefractOffset = mapNormal.xy;
            }
            
            vRefractOffset *= u_refractionScale * invDist;
            vDistortedScreenPos += (vRefractOffset * u_viewTexel.xy); 
        #else
            vDistortedScreenPos += (screenNormal.xy  * u_refractionScale * invDist) * u_viewTexel.xy;
        #endif
        
        vec4 vRefractionColor = texture2D(s_refractionMap, vDistortedScreenPos);
        
        ///////////////////////
        // Do blending in shader (blend mode is None with refraction)
        #ifdef USE_BLEND_MODE_ADD
            vFinalColor.xyz = vFinalColor.xyz + vRefractionColor.xyz;
        #endif
        #ifdef USE_BLEND_MODE_MUL
            vFinalColor.xyz = vFinalColor.xyz * vRefractionColor.xyz;
        #endif
        #ifdef USE_BLEND_MODE_MULX2
            vFinalColor.xyz = vFinalColor.xyz * vRefractionColor.xyz * 2.0;
        #endif
        #ifdef USE_BLEND_MODE_ALPHA
            vFinalColor.xyz = vFinalColor.xyz * vFinalColor.a + vRefractionColor.xyz * (1.0 - vFinalColor.a);
        #endif
        #ifdef USE_BLEND_MODE_PREMUL_ALPHA
            vFinalColor.xyz = vFinalColor.xyz + vRefractionColor.xyz * (1.0 - vFinalColor.a);
        #endif
    #endif
    
    
    ////////////////////
    //Enviroment Map
    #ifdef USE_CUBE_MAP     
        ///////////////////////////////
        //Calculate Reflection
        vec3 vEyeVec = normalize(v_position);
        
        float afEDotN = max(dot(-vEyeVec, screenNormal),0.0);
        float fFresnel = Fresnel(afEDotN, u_frenselBiasPow.x, u_frenselBiasPow.y);
        
        vec3 vEnvUv = reflect(vEyeVec, screenNormal);
        vEnvUv = mul(u_mtxInvViewRotation, vec4(vEnvUv,1)).xyz;
                    
        vec4 vReflectionColor = textureCube(s_envMap, vEnvUv);
        
        //Alpha for environment map
        if(0.0 < u_useCubeMapAlpha) {
            float fEnvMapAlpha = texture2D(s_envMapAlphaMap, v_texcoord0.xy).w;
            vReflectionColor *= fEnvMapAlpha;
        }
        
        vFinalColor.xyz += vReflectionColor.xyz * fFresnel * fFinalAlpha*u_lightLevel;
        
        ///////////////////////////////
        //Rim reflections

        if(u_rimLightMulPow.x >0.0) 
        {
            float fRimLight = dot(screenNormal, vec3(0.0, 0.0, -1.0));
            fRimLight = pow(1.0 - abs(fRimLight), u_rimLightMulPow.y) * u_rimLightMulPow.x;	
            
            vFinalColor.xyz += vReflectionColor.xyz * fRimLight * fFinalAlpha * u_lightLevel;
        }
    #endif
    
    gl_FragColor = vFinalColor;
}