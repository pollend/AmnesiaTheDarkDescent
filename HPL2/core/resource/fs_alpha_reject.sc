$input v_texcoord0

#include <common.sh>

SAMPLER2D(s_diffuseMap, 0);

uniform vec4 u_param;
#define u_alphaReject (u_param.x)

void main()
{
    vec4 color = texture2D(s_diffuseMap, v_texcoord0);
    if(color.w < u_alphaReject) {
        discard;
    }
    gl_FragColor = vec4(1.0);
}