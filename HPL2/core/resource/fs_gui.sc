$input v_texcoord0, v_color0, v_position

#include <common.sh>

uniform vec4 u_params;
#define u_hasTexture (u_params.x)
#define u_numClip    (u_params.y)

uniform vec4 u_clip_planes[4];

SAMPLER2D(s_diffuseMap, 0);

void main()
{
    vec4 color = v_color0;
    int numClipPlanes = int(u_numClip);
    for(int i = 0; i < numClipPlanes; i++)
    {
        vec4 plane = u_clip_planes[i];
        if(dot(plane, vec4(v_position, 1.0)) < 0.0)
        {
            discard;
        }
    }
    if(0.0 < u_hasTexture)
    {
        color *= texture2D(s_diffuseMap, v_texcoord0);
    }
	gl_FragColor = vec4(1.0);
}