
$input v_position, v_boxRay, v_texcoord0

#include <common.sh>

SAMPLER2D(s_depthMap, 0);

uniform vec4 avFogColor;
uniform vec4 u_params[4];

#define u_fogStart u_params[0].x
#define u_fogLength u_params[0].y
#define u_fogFalloffExp u_params[0].z

#define u_fogRayCastStart (vec3(u_params[0].w, u_params[1].x, u_params[1].y))
#define u_fogNegPlaneDistNeg (vec3(u_params[1].z, u_params[1].w, u_params[2].x))
#define u_fogNegPlaneDistPos (vec3(u_params[2].y, u_params[2].z, u_params[2].w))

#define u_useBacksize (u_params[3].x)
#define u_useOutsideBox (u_params[3].y)

float GetPlaneIntersection(vec3 avPlaneNormal, float afNegPlaneDist, float afFinalT)
{
    //Get T (amount of ray) to intersection
    float fMul  = dot(v_boxRay, avPlaneNormal);
    float fT = afNegPlaneDist / fMul;
    
    //Get the intersection and see if inside box
    vec3 vIntersection = abs(v_boxRay * fT + u_fogRayCastStart);
    if( all( lessThan(vIntersection, vec3(0.5001)) ) )
    {
        return max(afFinalT, fT);	
    }
    return afFinalT;
}

void main()
{
    vec4 vDepthVal = texture2D(s_depthMap, v_texcoord0.xy);
    float fDepth = -UnpackVec3ToFloat(vDepthVal.xyz) * afNegFarPlane;

    if(0.0 < u_useOutsideBox) {
        fDepth = fDepth +  v_position.z; //VertexPos is negative!
	
        float fFinalT = 0.0;
        if(0.0 < u_useBacksize) {
            fFinalT = GetPlaneIntersection(vec3(-1.0, 0.0, 0.0),	avNegPlaneDistNeg.x,	fFinalT);//Left
            fFinalT = GetPlaneIntersection(vec3(1.0, 0.0, 0.0), 	avNegPlaneDistPos.x,	fFinalT);//Right
            fFinalT = GetPlaneIntersection(vec3(0.0, -1.0, 0.0),	avNegPlaneDistNeg.y, 	fFinalT);//Bottom
            fFinalT = GetPlaneIntersection(vec3(0.0, 1.0, 0.0 ),	avNegPlaneDistPos.y, 	fFinalT);//Top
            fFinalT = GetPlaneIntersection(vec3(0.0, 0.0, -1.0),	avNegPlaneDistNeg.z, 	fFinalT);//Back
            fFinalT = GetPlaneIntersection(vec3(0.0, 0.0, 1.0), 	avNegPlaneDistPos.z,	fFinalT);//Front
            
            float fLocalBackZ = fFinalT * v_position.z -  v_position.z;
            fDepth = min(-fLocalBackZ, fDepth);
        }
        
    } else {
        if(0.0 < u_useBacksize) {
			fDepth = min(-v_position.z, fDepth);
		}
    }

    fDepth = min(- v_position.z, fDepth);
	float fAmount = max(fDepth / avFogStartAndLength.y,0.0);
	
	gl_FragColor.xyz = avFogColor.xyz;
	gl_FragColor.w = pow(fAmount, afFalloffExp) * avFogColor.w;
}
