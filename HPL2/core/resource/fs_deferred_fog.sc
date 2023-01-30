$input v_position, v_ray

#include <common.sh>

SAMPLER2D(s_positionMap, 0);

uniform vec4 u_fogColor;
uniform vec4 u_param[3];

#define u_fogStart u_param[0].x
#define u_fogLength u_param[0].y
#define u_fogFalloffExp u_param[0].z

#define u_fogRayCastStart (vec3(u_param[0].w, u_param[1].x, u_param[1].y))
#define u_fogNegPlaneDistNeg (vec3(u_param[1].z, u_param[1].w, u_param[2].x))
#define u_fogNegPlaneDistPos (vec3(u_param[2].y, u_param[2].z, u_param[2].w))

float GetPlaneIntersection(vec3 ray, vec3 avPlaneNormal, float afNegPlaneDist, float afFinalT)
{
    //Get T (amount of ray) to intersection
    float fMul  = dot(ray, avPlaneNormal);
    float fT = afNegPlaneDist / fMul;
    
    //Get the intersection and see if inside box
    vec3 vIntersection = abs(ray * fT + u_fogRayCastStart);
    if( all( lessThan(vIntersection, vec3(0.5001,0.5001,0.5001)) ) )
    {
        return max(afFinalT, fT);	
    }
    return afFinalT;
}

void main()
{
    vec2 ndc = gl_FragCoord.xy * u_viewTexel.xy;
    float fDepth = -texture2D(s_positionMap, ndc).z;

   #ifdef USE_OUTSIDE_BOX
        fDepth = fDepth +  v_position.z; //VertexPos is negative!
    
        float fFinalT = 0.0;
        #ifdef USE_BACK_SIDE
            fFinalT = GetPlaneIntersection(v_ray, vec3(-1.0, 0.0, 0.0),	u_fogNegPlaneDistNeg.x,	fFinalT);//Left
            fFinalT = GetPlaneIntersection(v_ray, vec3(1.0, 0.0, 0.0), 	u_fogNegPlaneDistPos.x,	fFinalT);//Right
            fFinalT = GetPlaneIntersection(v_ray, vec3(0.0, -1.0, 0.0),	u_fogNegPlaneDistNeg.y, fFinalT);//Bottom
            fFinalT = GetPlaneIntersection(v_ray, vec3(0.0, 1.0, 0.0 ),	u_fogNegPlaneDistPos.y, fFinalT);//Top
            fFinalT = GetPlaneIntersection(v_ray, vec3(0.0, 0.0, -1.0),	u_fogNegPlaneDistNeg.z, fFinalT);//Back
            fFinalT = GetPlaneIntersection(v_ray, vec3(0.0, 0.0, 1.0), 	u_fogNegPlaneDistPos.z,	fFinalT);//Front
            
            float fLocalBackZ = fFinalT * v_position.z -  v_position.z;
            fDepth = min(-fLocalBackZ, fDepth);
        #endif
        
    #else
        #ifdef USE_BACK_SIDE
            fDepth = min(-v_position.z, fDepth);
        #endif
    #endif

    fDepth = min(- v_position.z, fDepth);
    float fAmount = max(fDepth / u_fogLength,0.0);
    
    gl_FragColor.xyz = u_fogColor.xyz;
    gl_FragColor.w = pow(fAmount, u_fogFalloffExp) * u_fogColor.w;
}
