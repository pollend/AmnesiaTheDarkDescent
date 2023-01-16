$input v_texcoord0, v_normal, v_tangent, v_bitangent, v_view, v_position

#include <common.sh>

SAMPLERCUBE(s_envMap, 0);
SAMPLER2D(s_normalMap, 1);
SAMPLER2D(s_specularMap, 2);
SAMPLER2D(s_heightMap, 3);
SAMPLER2D(s_diffuseMap, 4);
SAMPLER2D(s_envMapAlphaMap, 5);

uniform mat4 u_mtxInvViewRotation;
uniform vec4 u_param[2];
#define u_heightMapScale (u_param[0].x)
#define u_heightMapBias (u_param[0].y)
#define u_fresnelBias (u_param[0].z)
#define u_fresnelPow (u_param[0].w)

#define u_useCubeMapAlpha (u_param[1].x)

void main()
{
    vec4 diffuseColor = vec4(0.0, 0.0, 0.0, 0.0);
    vec2 texCoord = v_texcoord0.xy;
    vec3 normalizedView = normalize(v_view);

    #ifdef USE_PARALLAX_MAPS
        
        //Get give normalizedView the length so it reaches bottom.
        normalizedView *= vec3(1.0 / normalizedView.z);	
        
        //Apply scale and bias
        normalizedView.xy *= u_heightMapScale;
        //vec2 vBiasPosOffset = vEyeVec.xy * avHeightMapScaleAndBias.y; <- not working! because the ray casting buggers out when u are really close!
			
        float heightDepth = 0.0;

        vec3 heightMapPos = vec3(texCoord.xy, 0.0);

        //Determine number of steps based on angle between normal and eye
        float fSteps = floor((1.0 - dot(normalizedView, normalize(v_normal)) ) * 18.0) + 1.0; 
        
        // Do a linear search to find the first intersection
        {
            normalizedView /= fSteps;
            int iterations = max(20, int(fSteps));
            for(int i = 0; i < iterations; i++) 
            { 
                float fDepth = texture2D(s_heightMap, texCoord.xy).w; 
                if(heightDepth < fDepth) {
                    heightMapPos += normalizedView;
                } 
            } 
        }

        // Do a binary search to find the exact intersection
        {
            const int lSearchSteps = 6; 
            for(int i=0; i<lSearchSteps; i++) 
            { 
                float fDepth = texture2D(s_heightMap, texCoord.xy).w;
                if(heightMapPos.z < fDepth) {
                    heightMapPos += normalizedView; 
                }
                
                normalizedView *= 0.5; 
                heightMapPos -= normalizedView; 
            } 
        }
        texCoord.xy = heightMapPos.xy;
    #endif

    diffuseColor = texture2D(s_diffuseMap, texCoord.xy);
    
    vec3 screenNormal = vec3(0, 0, 0);
    
    #ifdef USE_NORMAL_MAPS
        vec3 vNormal = texture2D(s_normalMap, texCoord).xyz - 0.5; //No need for full unpack x*2-1, becuase normal is normalized. (but now we do not normalize...)
        screenNormal = normalize(vNormal.x * v_tangent + vNormal.y * v_bitangent + vNormal.z * v_normal);
    #else
        screenNormal = normalize(v_normal);
    #endif

    #ifdef USE_ENVMAP
        vec3 vEnvUv = reflect(normalizedView, screenNormal);
        vEnvUv = (u_mtxInvViewRotation * vec4(vEnvUv,1)).xyz;
                    
        vec4 reflectionColor = textureCube(s_envMap, vEnvUv);
        
        float afEDotN = max(dot(-normalizedView, screenNormal),0.0);
        float fFresnel = Fresnel(afEDotN, u_fresnelBias, u_fresnelPow);
        
        if(0.0 < u_useCubeMapAlpha) {
            reflectionColor *= texture2D(s_envMapAlphaMap, texCoord).w;
        }
                
        gl_FragData[0] = diffuseColor + reflectionColor * fFresnel;
    #else
        gl_FragData[0] = diffuseColor;
    #endif
    
    gl_FragData[1].xyz = screenNormal; 
    gl_FragData[2].xyz = v_position;
    #ifdef USE_SPECULAR_MAPS
        gl_FragData[3].xy = texture2D(s_specularMap, texCoord).xy;
    #else
        gl_FragData[3].xy = vec2(0.0);
    #endif
}