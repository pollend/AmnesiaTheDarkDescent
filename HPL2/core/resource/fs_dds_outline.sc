$input v_texcoord0

#include <common.sh>

SAMPLER2D(s_diffuseMap, 0);

uniform vec4 u_param;
#define u_color (u_param.xyz)
#define u_useAlpha (u_param.z)

void main()
{
    if(0.0 < u_useAlpha) {
        gl_FragColor.xyz = u_color * texture2D(s_diffuseMap, v_texcoord0).w;	
    } else {
        gl_FragColor.xyz = u_color;	
    }
    gl_FragColor = vec4(1.0);
}