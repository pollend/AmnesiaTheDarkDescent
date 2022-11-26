// $output v_texcoord0

// varying vec3 gvNormal;
// varying vec3 gvTangent;
// varying vec3 gvBinormal;
// varying vec4 gvColor;
// varying float gfLinearDepth;
// varying vec3 gvLightPosition;
// varying vec4 gvLightColor;
// varying float gfLightRadius;
// varying vec3 gvVertexPos;	
// varying vec3 gvFarPlanePos;
// varying vec3 gvTangentEyePos;

// UseUv
// UseNormals
// UseDepth
// VirtualPositionAddScale

#define Feature_Diffuse_NormalMaps 0x00000001
#define Feature_Diffuse_Specular 0x00000002
#define Feature_Diffuse_Parallax 0x00000004
#define Feature_Diffuse_UvAnimation 0x00000008
#define Feature_Diffuse_Skeleton 0x00000010
#define Feature_Diffuse_EnvMap 0x00000020
#define Feature_Diffuse_CubeMapAlpha 0x00000040

uniform vec3 u_params;

uniform vec4 u_params;
#define u_features  	floatBitsToUint(u_params.x)
#define afInvFarPlane  u_params.y
#define afNegFarPlane  u_params.z

uniform float afInvFarPlane;
uniform float afNegFarPlane;
uniform mat4 a_mtxUV;
uniform mat4 a_mtxBoxInvViewModelRotation;

void main()
{	
	//////////////////////
	// Position
	gl_Position = ftransform();
	gvNormal = normalize(gl_NormalMatrix * gl_Normal);
	
	
	//////////////////////
	// Color
	@ifdef UseColor
		gvColor = gl_Color;
	@endif
	
	@ifdef UseUvAnimation
		gl_TexCoord[0] = a_mtxUV * gl_MultiTexCoord0;
	@else
		gl_TexCoord[0] = gl_MultiTexCoord0;
	@endif
	@ifdef UseUvCoord1
		gl_TexCoord[1] = gl_MultiTexCoord1;
	@endif


	//////////////////////
	// Normalmapping
	@ifdef UseNormalMapping
		//To consider: Is gl_NormalMatrix correct here?
		gvTangent = normalize(gl_NormalMatrix * gl_MultiTexCoord1.xyz);
		
		//Need to do it in model space (and not view) because reflection normal mapping will fail otherwise!
		gvBinormal = normalize(gl_NormalMatrix * cross(gl_Normal,gl_MultiTexCoord1.xyz) * gl_MultiTexCoord1.w); 
	@endif
	
	//////////////////////
	// Parallax
	@ifdef UseParallax
		vec3 vViewEyeVec =  (gl_ModelViewMatrix * gl_Vertex).xyz;
		
		gvTangentEyePos.x = dot(vViewEyeVec, gvTangent);
		gvTangentEyePos.y = dot(vViewEyeVec, gvBinormal);
		gvTangentEyePos.z = dot(-vViewEyeVec, gvNormal);
		
		//Do not normalize yet! Do that in the fragment shader.		
	@endif

	//////////////////////
	// Deferring (G-Buffer)
	@ifdef Deferred_32bit
		gfLinearDepth = -(gl_ModelViewMatrix * gl_Vertex).z * afInvFarPlane; //Do not use near plane! Doing like this will make the calcs simpler in the light shader.
	@endif
	
	@ifdef Deferred_64bit || UseVertexPosition || UseEnvMap || UseFog || UseRefraction
		gvVertexPos = (gl_ModelViewMatrix * gl_Vertex).xyz;
	@endif
	
	//////////////////////
	// Deferring (Lights)
	@ifdef DeferredLight
	
		@ifdef Deferred_32bit
			////////////////////////////
			//Light are rendered as shapes
			@ifdef UseDeferredLightShapes
				//Project the position to the farplane
				vec3 vPos = (gl_ModelViewMatrix * gl_Vertex).xyz;
				
				//Spotlight will divided in fragment shader
				@ifdef DivideInFrag
					gvFarPlanePos = vPos;
					gvFarPlanePos.xy *= afNegFarPlane;
				//Point light does division now
				@else
					vec2 vTanXY = vPos.xy / vPos.z;
					
					gvFarPlanePos.xy = vTanXY * afNegFarPlane;
					gvFarPlanePos.z = afNegFarPlane;
				@endif
			////////////////////////////
			//Light are 2D quads
			@else
				//No need to project, postion is already at far plane.
				gvFarPlanePos = gl_Vertex.xyz;
			@endif
		@endif
		
		////////////////////////////
		//Batching is used
		@ifdef UseBatching
			gvLightPosition = gl_MultiTexCoord0.xyz;
			gvLightColor = 	gl_Color;
			gfLightRadius = gl_MultiTexCoord1.x;			
		@endif
	@endif
}