$input v_color0, v_texcoord0, v_normal, v_tangent, v_bitangent, v_view, v_position

#include <common.sh>

SAMPLER2D(s_diffuseMap, 0);
SAMPLER2D(s_normalMap, 1);
SAMPLER2D(s_refractionMap, 2);
SAMPLER2D(s_envMapAlphaMap, 3);

SAMPLERCUBE(s_envMap, 4);

uniform mat4 u_mtxInvViewRotation;
uniform vec4 u_fogColor;

uniform vec4 u_params[6];
#define u_useBlendModeAdd (u_params[0].x)
#define u_useBlendModeMul (u_params[0].y)
#define u_useBlendModeMulX2 (u_params[0].z)
#define u_useBlendModeAlpha (u_params[0].w)

#define u_useBlendModePremulAlpha (u_params[1].x)
#define u_useEvnMap (u_params[1].y)
#define u_useDiffuseMap (u_params[1].z)
#define u_useFog (u_params[1].w)

#define u_useScreenNormal (u_params[2].x)
#define u_useNormalMap (u_params[2].y)
#define u_useRefraction (u_params[2].z)
#define u_fogStart (u_params[2].w)

#define u_fogLength (u_params[3].x)
#define u_alpha (u_params[3].y)
#define u_lightLevel (u_params[3].z)
#define u_oneMinusFogAlpha (u_params[3].w)

#define u_frenselBiasPow (u_params[4].xy)
#define u_rimLightMulPow (u_params[4].zw)

#define u_falloffExp (u_params[5].x)
#define u_refractionScale (u_params[5].y)
#define u_useCubeMapAlpha (u_params[5].z)


void main()
{
    vec4 vFinalColor = vec4(0.0, 0.0 ,0.0, 1.0);
 
    if(0.0 < u_useDiffuseMap) { 
        vFinalColor = texture2D(s_diffuseMap, v_texcoord0.xy) * v_color0;
    }
    ////////////////////
    //Fog

    float fFinalAlpha = u_alpha;
    if(0.0 < u_useFog) { 
        float fFogAmount = (-v_position.z - u_fogStart) / u_fogLength;
        fFogAmount = clamp(fFogAmount, 0.0, 1.0);
        fFogAmount = pow(fFogAmount, u_falloffExp);
        
        fFinalAlpha = u_oneMinusFogAlpha * fFogAmount + (1.0 - fFogAmount);
        fFinalAlpha *= u_alpha;
    }
    
    
    ////////////////////
    //Calculate new color based on Alpha and Blend mode
    if(0.0 < u_useBlendModeAdd) {
        vFinalColor.xyz *= fFinalAlpha*u_lightLevel;
    } else if(0.0 < u_useBlendModeMul) {
        vFinalColor.xyz += (vec3(1.0) - vFinalColor.xyz) * (1.0-fFinalAlpha);
    } else if(0.0 < u_useBlendModeMulX2) {
        float fBlendMulAlpha = u_lightLevel*fFinalAlpha;
        vFinalColor.xyz = vFinalColor.xyz*fBlendMulAlpha + vec3(0.5)*(1.0 - fBlendMulAlpha);
    } else  if(0.0 < u_useBlendModeAlpha) {
        vFinalColor.a *= fFinalAlpha;
        vFinalColor.xyz *= u_lightLevel;
    } else if(0.0 < u_useBlendModePremulAlpha) {
        vFinalColor *= fFinalAlpha;
        vFinalColor.xyz *= u_lightLevel;
    }
    
    vec3 mapNormal = vec3(0);
    vec3 screenNormal = vec3(0);
    if(0.0 < u_useNormalMap) { 
        mapNormal = texture2D(s_normalMap, v_texcoord0.xy).xyz*2.0 - 1.0; 	
        screenNormal = normalize(mapNormal.x * v_tangent + mapNormal.y * v_bitangent + mapNormal.z * v_normal);
    }

    ////////////////////
    //Refraction
    if(0.0 < u_useRefraction) {
        float invDist = min(1.0/v_position.z, 10.0);
            
        ///////////////////////
        // Sample refaraction map (using distorted coords)
        vec2 vDistortedScreenPos = v_texcoord0.xy;
        if(0.0 < u_useNormalMap) { 
            
            //Should the screen normal or texture normal be used?
            vec2 vRefractOffset;
            if(0.0 < u_useScreenNormal) {
                vRefractOffset = screenNormal.xy;
            } else {
                vRefractOffset = mapNormal.xy;
            }
            
            vRefractOffset *= u_refractionScale * invDist;
            vDistortedScreenPos += vRefractOffset; 
        } else {
            vDistortedScreenPos += screenNormal.xy  * u_refractionScale * invDist;
        }
        
        vec4 vRefractionColor = texture2D(s_refractionMap, vDistortedScreenPos);
        
        ///////////////////////
        // Do blending in shader (blend mode is None with refraction)		
        if(0.0 < u_useBlendModeAdd) {
            vFinalColor.xyz = vFinalColor.xyz + vRefractionColor.xyz;
        } else if(0.0 < u_useBlendModeMul) {
            vFinalColor.xyz = vFinalColor.xyz * vRefractionColor.xyz;
        } else if(0.0 < u_useBlendModeMulX2) {
            vFinalColor.xyz = vFinalColor.xyz * vRefractionColor.xyz * 2.0;
        } else  if(0.0 < u_useBlendModeAlpha) {
            vFinalColor.xyz = vFinalColor.xyz * vFinalColor.a + vRefractionColor.xyz * (1.0 - vFinalColor.a);
        } else if(0.0 < u_useBlendModePremulAlpha) {
            vFinalColor.xyz = vFinalColor.xyz + vRefractionColor.xyz * (1.0 - vFinalColor.a);
        }
    }
    
    
    ////////////////////
    //Enviroment Map
    if(0.0 < u_useEvnMap) {
    
        ///////////////////////////////
        //Calculate Reflection
        vec3 vEyeVec = normalize(v_position);
        
        float afEDotN = max(dot(-vEyeVec, screenNormal),0.0);
        float fFresnel = Fresnel(afEDotN, u_frenselBiasPow.x, u_frenselBiasPow.y);
        
        vec3 vEnvUv = reflect(vEyeVec, screenNormal);
        vEnvUv = (u_mtxInvViewRotation * vec4(vEnvUv,1)).xyz;
                    
        vec4 vReflectionColor = textureCube(s_envMap,vEnvUv);
        
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
        
    }
    
    gl_FragColor = vFinalColor;
}