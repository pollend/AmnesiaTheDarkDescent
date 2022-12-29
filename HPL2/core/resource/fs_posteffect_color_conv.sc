
$input v_texcoord0

SAMPLER2D(s_convMap, 0);
SAMPLER2D(s_diffuseMap, 1);

uniform vec4 u_params;
#define u_alphaFade (u_params.x)

void main()
{
    vec3 diffuseColor = texture2D(s_diffuseMap, v_texcoord0).xyz;

    vec3 outputColor =  vec3(texture2D(s_convMap, vec2(diffuseColor.x, 0)).x,
                    texture2D(s_convMap, vec2(diffuseColor.y, 0)).y,
                    texture2D(s_convMap, vec2(diffuseColor.z, 0)).z);
    gl_FragColor.xyz = (outputColor * u_alphaFade) + (diffuseColor * (1.0 - u_alphaFade));
}