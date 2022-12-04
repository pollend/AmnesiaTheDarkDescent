$input v_texcoord0, v_normal, v_tangent, v_bitangent, v_view, v_position

#include <common.sh>

SAMPLER2D(s_normalMap, 0);
SAMPLER2D(s_specularMap, 1);
SAMPLER2D(s_heightMap, 2);
SAMPLER2D(s_diffuseMap, 3);
SAMPLER2D(s_envMapAlphaMap, 4);

SAMPLERCUBE(aEnvMap, 5);

uniform vec4 u_params[3];
#define u_heightMapScaleAndBias (u_params[0].xy)
#define u_frenselBiasPow (u_params[0].zw)

#define u_useNormalMap (u_params[1].x)
#define u_useSpecular (u_params[1].y)
#define u_useEnvMap (u_params[1].z)
#define u_useCubeMapAlpha (u_params[1].w)

#define u_useParallax (u_params[2].x)

uniform mat4 a_mtxInvViewRotation;

///////////////////////////////
// Main program
void main()
{
	//////////////////////////////////
	//Diffuse
    vec4 diffuseColor = vec4(0.0, 0.0, 0.0, 1.0);
    vec2 texCoord = v_texcoord0.xy;

    if(0.0 < u_useParallax) {
        vec3 eyeVec = normalize(v_view);
        
        //Get give eyevec the length so it reaches bottom.
        eyeVec *= vec3(1.0 / eyeVec.z);	
        
        //Apply scale and bias
        eyeVec.xy *= u_heightMapScaleAndBias.x;
        //vec2 vBiasPosOffset = eyeVec.xy * u_heightMapScaleAndBias.y; <- not working! because the ray casting buggers out when u are really close!
        
        //Get the postion to start the search from. 0 since we start at the top.
        vec3 parallaxPosCoord = vec3(v_texcoord0.xy, 0.0); 
        
        //Determine number of steps based on angle between normal and eye
        float fSteps = floor((1.0 - dot(eyeVec, normalize(v_normal)) ) * 18.0) + 2.0; 
        
        {
            eyeVec /= fSteps;
            for(float i = 0.0; i < (fSteps - 1.0); i++) 
            { 
                float fDepth = texture2D(s_heightMap, parallaxPosCoord.xy).w; 
                if(parallaxPosCoord.z < fDepth) {
                    parallaxPosCoord += eyeVec;
                } 
            } 
        }

        {
            const int lSearchSteps = 6; 
            for(int i=0; i<lSearchSteps; i++) 
            { 
                float fDepth = texture2D(s_heightMap, parallaxPosCoord.xy).w;
                if(parallaxPosCoord.z < fDepth) {
                    parallaxPosCoord += eyeVec; 
                }
                
                eyeVec *= 0.5; 
                parallaxPosCoord -= eyeVec; 
            } 
        }

        //Do a linear search to find the first intersection.
        // RayLinearIntersectionSM3(s_heightMap,fSteps, parallaxPosCoord,  eyeVec);
        
        //Do a binary search between start and first intersection to pinpoint the postion.		
        //RayBinaryIntersection(s_heightMap, parallaxPosCoord,  eyeVec);
        
		diffuseColor = texture2D(s_diffuseMap, parallaxPosCoord.xy);
        texCoord = parallaxPosCoord.xy;
	} else {
        diffuseColor = texture2D(s_diffuseMap, texCoord.xy);
	}
	
    vec3 screenNormal = vec3(0, 0, 0);
	if (0.0 < u_useNormalMap) {
		vec3 vNormal = texture2D(s_normalMap, texCoord).xyz - 0.5; //No need for full unpack x*2-1, becuase normal is normalized. (but now we do not normalize...)
		screenNormal = normalize(vNormal.x * v_tangent + vNormal.y * v_bitangent + vNormal.z * v_normal);
	} else {
		screenNormal = normalize(v_normal);
	}

	if(0.0 < u_useEnvMap) {
		vec3 normalizedView = normalize(v_view);	
	
		vec3 vEnvUv = reflect(normalizedView, screenNormal);
		vEnvUv = (a_mtxInvViewRotation * vec4(vEnvUv,1)).xyz;
					
		vec4 reflectionColor = textureCube(aEnvMap, vEnvUv);
		
		float afEDotN = max(dot(-normalizedView, screenNormal),0.0);
		float fFresnel = Fresnel(afEDotN, u_frenselBiasPow.x, u_frenselBiasPow.y);
		
		if(0.0 < u_useCubeMapAlpha) {
			float fEnvMapAlpha = texture2D(s_envMapAlphaMap, texCoord).w;
			reflectionColor *= fEnvMapAlpha;
		}
				
		gl_FragData[0] = diffuseColor + reflectionColor * fFresnel;
	} else {
        gl_FragData[0] = diffuseColor;
    }
	
    gl_FragData[1].xyz = screenNormal; 
    gl_FragData[2].xyz = v_position;
    if(u_useSpecular < 0.0) {
        gl_FragData[3].xy = texture2D(s_specularMap, texCoord).xy;
    } else {
        gl_FragData[3].xy = vec2(0.0);
    }
}