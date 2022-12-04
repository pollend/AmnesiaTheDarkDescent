

$input v_texcoord0, v_normal, v_tangent, v_bitangent, v_position

#include <common.sh>

SAMPLER2D(s_diffuseMap, 0);
SAMPLER2D(s_normalMap, 1);
SAMPLER2D(s_refractionMap, 2);
SAMPLER2D(s_reflectionMap, 3);

SAMPLERCUBE(s_envMap, 4);

uniform mat4 u_mtxInvViewRotation;
uniform vec4 u_fogColor;

uniform vec4 u_params[5];
#define u_frenselBiasPow (u_params[0].xy)
#define u_fogStartAndLength (u_params[0].zw)

#define u_reflectionMapSizeMul (u_params[1].xy)
#define u_reflectionFadeStartAndLength (u_params[1].zw)

#define u_falloffExp (u_params[2].x)
#define u_afT (u_params[2].y)
#define u_afWaveAmplitude (u_params[2].z)
#define u_afWaveFreq (u_params[2].w)

#define u_afRefractionScale (u_params[3].x)
#define u_useFog (u_params[3].y)
#define u_useRefraction (u_params[3].z)
#define u_useReflection (u_params[3].w)

#define u_useReflectionFading (u_params[4].x)
#define u_useCubeMapReflection (u_params[4].y)
#define u_useRefractionEdgeCheck (u_params[4].z)

void main()
{
	///////////////////////////////
	//Get the two uv coords
	float fT1 = u_afT*0.8;
	vec2 vUv1 = v_texcoord0.xy + u_afT*0.01f;
	vUv1.x += sin(fT1 + vUv1.y * u_afWaveFreq)* u_afWaveAmplitude;
	vUv1.y += sin(fT1 + vUv1.x * u_afWaveFreq)* u_afWaveAmplitude;
	
	float fT2 = u_afT*-2.6;
	vec2 vUv2 = v_texcoord0.xy + u_afT*-0.012f;
	vUv2.x += sin(fT2 + vUv2.y * u_afWaveFreq*1.2) * u_afWaveAmplitude*0.75;
	vUv2.y += sin(fT2 + vUv2.x * u_afWaveFreq*1.2) * u_afWaveAmplitude*0.75;
	
	///////////////////////////////
	//Get the normals and combine into final normal
	// (No need for full unpack since there is a normalize later)
	vec3 vNormal1 = texture2D(s_normalMap, vUv1).xyz-0.5;
	vec3 vNormal2 = texture2D(s_normalMap, vUv2).xyz-0.5;
	
	vec3 vFinalNormal = normalize(vNormal1*0.7 + vNormal2*0.3);
		
	///////////////////////////////
	//Get the diffuse color
	vec4 surfaceColor = texture2D(s_diffuseMap, vUv1);
	
	vec4 vRefractionColor = vec4(1);
	if(0.0 < u_useRefraction) {
		float fInvDist = min(1.0 / v_position.z, 10.0);
		vec2 vDistortedScreenPos = gl_FragCoord.xy + vFinalNormal.xy * u_afRefractionScale * fInvDist;
		
		vec4 vRefractionColor = texture2D(s_refractionMap, vDistortedScreenPos);
		
		if(0.0 < u_useRefractionEdgeCheck && vRefractionColor.w <0.5) {
			vRefractionColor = texture2D(s_refractionMap, gl_FragCoord.xy);
		}
	}
	
	
	vec4 vReflectionColor = vec4(0);
	float fFresnel = 1.0;
	if(0.0 < u_useReflection) {
				
		//////////////////
		//Fresnel
		vec3 vScreenNormal = normalize(vFinalNormal.x * v_tangent + vFinalNormal.y * v_bitangent + vFinalNormal.z * v_normal);
		vec3 vEyeVec = normalize(v_position);
		
		float afEDotN = max(dot(-vEyeVec, vScreenNormal),0.0);
		fFresnel = Fresnel(afEDotN, u_frenselBiasPow.x, u_frenselBiasPow.y);
		
		if(0.0 < u_useReflectionFading) {
			fFresnel *= 1.0 - clamp( (v_position.z - u_reflectionFadeStartAndLength.x) / u_reflectionFadeStartAndLength.y, 0.0, 1.0);
		}

		//////////////////
		//Cubemap
		if(0.0 < u_useCubeMapReflection) {
			vec3 vEnvUv = reflect(vEyeVec, * vec4(vEnvUv,1)).xyz;
			vReflectionColor = textureCube(s_envMap,vEnvUv);
		} else {
			vReflectionColor = texture2D(s_reflectionMap, vDistortedScreenPos * u_reflectionMapSizeMul);
		}
		
	}
	
	///////////////////////////////
	//Caclulate the final color
	if(0.0 < u_useFog) {
		float fFogAmount = (-v_position.z - u_fogStartAndLength.x) / u_fogStartAndLength.y;
		fFogAmount = clamp(fFogAmount, 0.0, 1.0);
		fFogAmount = pow(fFogAmount, u_falloffExp) * u_fogColor.a;

		vec3 vLightDir = normalize(vec3(0.5, 0.5, 0.5));
		float fLDotN = max(dot(vLightDir, vFinalNormal),0.0);
		float fDiffuse =  fLDotN * 0.5 + 0.5;
		float fSpecular = pow(fLDotN,16.0);

		if(0.0 < u_useRefraction) {
			if(0.0 < u_useReflection) {
				gl_FragColor.xyz = (surfaceColor.xyz * vRefractionColor.xyz + vReflectionColor.xyz * fFresnel) * (1.0-fFogAmount) + u_fogColor.xyz*fFogAmount;
				gl_FragColor.w = 1.0;
			} else {
				gl_FragColor.xyz = (surfaceColor.xyz * vRefractionColor.xyz * fDiffuse + vec3(fSpecular)) * (1.0-fFogAmount) + u_fogColor.xyz*fFogAmount;
				gl_FragColor.w = 1.0;
			}
		} else {
			gl_FragColor.xyz = (surfaceColor.xyz * vRefractionColor.xyz *fDiffuse + vec3(fSpecular)) * (1.0-fFogAmount) + vec3(fFogAmount);
			gl_FragColor.w = 1.0;
		}
	} else {
		if(0.0 < u_useReflection) {
			gl_FragColor.xyz = vRefractionColor.xyz*surfaceColor.xyz + vReflectionColor.xyz * fFresnel;
			gl_FragColor.w = 1.0;
		} else {
			gl_FragColor.xyz = surfaceColor.xyz * vRefractionColor.xyz *fDiffuse + vec3(fSpecular);
			gl_FragColor.w = 1.0;
		}
	}
}