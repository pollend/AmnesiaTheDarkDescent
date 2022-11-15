$input v_pos, v_normal, v_tangent, v_bitangent, v_texcoord0

#include "./common.sh"

SAMPLER2D(s_normalMap, 2);
SAMPLER2D(s_diffuseMap, 1);
#if UseRefraction
	SAMPLER2D(s_refractionMap, 3);
#endif
#if UseReflection
	#if UseCubeMapReflection
		SAMPLERCUBE(s_envMap, 4);
	#else
		SAMPLER2D(s_reflectionMap, 4);
	#endif
#endif

////////////////////////////////
// Arguments
uniform float afT;
uniform float afWaveAmplitude;
uniform float afWaveFreq;
uniform float afRefractionScale;

#if UseFog
	uniform vec2 avFogStartAndLength;
	uniform vec4 avFogColor;
	uniform float afFalloffExp;
#endif	

#if UseReflection
	uniform vec2 avFrenselBiasPow;

	#if UseCubeMapReflection
		uniform mat4 a_mtxInvViewRotation;
	#else
		uniform vec2 avReflectionMapSizeMul;
	#endif
	
	#if UseReflectionFading
		uniform vec2 avReflectionFadeStartAndLength;
	#endif
#endif


////////////////////////////////
// Main
void main()
{
	///////////////////////////////
	//Get the two uv coords
	float fT1 = afT*0.8;
	vec2 vUv1 = v_texcoord0.xy+afT*0.01f;
	vUv1.x += sin(fT1 + vUv1.y * afWaveFreq)* afWaveAmplitude;
	vUv1.y += sin(fT1 + vUv1.x * afWaveFreq)* afWaveAmplitude;
	
	float fT2 = afT*-2.6;
	vec2 vUv2 = v_texcoord0.xy+afT*-0.012f;
	vUv2.x += sin(fT2 + vUv2.y * afWaveFreq*1.2) * afWaveAmplitude*0.75;
	vUv2.y += sin(fT2 + vUv2.x * afWaveFreq*1.2) * afWaveAmplitude*0.75;
	
	///////////////////////////////
	//Get the normals and combine into final normal
	// (No need for full unpack since there is a normalize later)
	vec3 vNormal1 = texture2D(s_normalMap, vUv1).xyz-0.5;
	vec3 vNormal2 = texture2D(s_normalMap, vUv2).xyz-0.5;
	
	vec3 vFinalNormal = normalize(vNormal1*0.7 + vNormal2*0.3);
		
	///////////////////////////////
	//Get the diffuse color
	vec4 vSurfaceColor = texture2D(s_diffuseMap, vUv1);
	
	///////////////////////////////
	//Get the fog amount
	#if UseFog
		float fFogAmount = (-v_pos.z - avFogStartAndLength.x)/ avFogStartAndLength.y;
		fFogAmount = clamp(fFogAmount, 0.0, 1.0);
		fFogAmount = pow(fFogAmount, afFalloffExp) * avFogColor.a;
	#endif	
	
	
	///////////////////////////////
	//Get the refraction color
	#if UseRefraction
		float fInvDist = min(1.0/v_pos.z, 10.0);
		vec2 vDistortedScreenPos = gl_FragCoord.xy + vFinalNormal.xy * afRefractionScale * fInvDist;
		
		vec4 vRefractionColor = texture2DRect(s_refractionMap, vDistortedScreenPos);
		
		#if UseRefractionEdgeCheck
			if(vRefractionColor.w <0.5) 
				vRefractionColor = texture2DRect(s_refractionMap, gl_FragCoord.xy);
		#endif
	#else
		vec4 vRefractionColor = vec4(1);
	#endif
	
	///////////////////////////////
	//Get the reflection color
	#if UseReflection
		//////////////////
		//Reflection fading
		#if UseReflectionFading
			float fReflFade = 1.0 - clamp( (v_pos.z - avReflectionFadeStartAndLength.x) / avReflectionFadeStartAndLength.y, 0.0, 1.0);
		#endif	
				
		//////////////////
		//Fresnel
		vec3 vScreenNormal = normalize(vFinalNormal.x * v_tangent + vFinalNormal.y * v_bitangent + vFinalNormal.z * v_normal);
		vec3 vEyeVec = normalize(v_pos);
		
		float afEDotN = max(dot(-vEyeVec, vScreenNormal),0.0);
		float fFresnel = Fresnel(afEDotN, avFrenselBiasPow.x, avFrenselBiasPow.y);
		
		#if UseReflectionFading
			fFresnel *= fReflFade;
		#endif
					
		#if UseCubeMapReflection
			//////////////////
			//Cubemap
			vec3 vEnvUv = reflect(vEyeVec, vScreenNormal);
			vEnvUv = (a_mtxInvViewRotation * vec4(vEnvUv,1)).xyz;
					
			vec4 vReflectionColor = textureCube(s_envMap,vEnvUv);
		#else
			//////////////////
			//World reflection
			vec4 vReflectionColor = texture2DRect(s_reflectionMap, vDistortedScreenPos * avReflectionMapSizeMul);
		#endif
	#else
		vec3 vLightDir = normalize(vec3(0.5, 0.5, 0.5));
		float fLDotN = max(dot(vLightDir, vFinalNormal),0.0);
		float fDiffuse =  fLDotN * 0.5 + 0.5;
		float fSpecular = pow(fLDotN,16.0);
	#endif
	
	///////////////////////////////
	//Calculate the final color
	#if UseFog
		#if UseRefraction
			#if UseReflection
				gl_FragColor.xyz = (vRefractionColor.xyz*vSurfaceColor.xyz + vReflectionColor.xyz*fFresnel) * (1.0-fFogAmount) + avFogColor.xyz*fFogAmount;
				gl_FragColor.w = 1.0;
			#else
				gl_FragColor.xyz = (vSurfaceColor.xyz * vRefractionColor.xyz *fDiffuse + vec3(fSpecular)) * (1.0-fFogAmount) + avFogColor.xyz*fFogAmount;
				gl_FragColor.w = 1.0;
			#endif
		#else
			gl_FragColor.xyz = (vSurfaceColor.xyz * vRefractionColor.xyz *fDiffuse + vec3(fSpecular)) * (1.0-fFogAmount) + vec3(fFogAmount);
			gl_FragColor.w = 1.0;
		#endif
	#else
		#if UseReflection
			gl_FragColor.xyz = vRefractionColor.xyz*vSurfaceColor.xyz + vReflectionColor.xyz*fFresnel;
			gl_FragColor.w = 1.0;
		#else
			gl_FragColor.xyz = vSurfaceColor.xyz * vRefractionColor.xyz *fDiffuse + vec3(fSpecular);
			gl_FragColor.w = 1.0;
		#endif
	#endif

    
}
