$input v_texcoord0, v_normal, v_tangent, v_bitangent, v_position

#include <common.sh>

SAMPLER2D(s_diffuseMap, 0);

void main()
{
    vec2 texCoord = v_texcoord0.xy;
    gl_FragData[0] = texture2D(s_diffuseMap, texCoord.xy);
}