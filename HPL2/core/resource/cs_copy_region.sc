/*
 * Copyright 2018 Kostas Anagnostou. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#include "bgfx_compute.sh"

SAMPLER2D(s_diffuseMap, 0);
IMAGE2D_WR(s_copyImageUAV, rgba8, 1);

uniform vec4 u_copyRegion;

NUM_THREADS(16, 16, 1)
void main()
{
	// this shader can be used to both copy a mip over to the output and downscale it.

	ivec2 coord = ivec2(gl_GlobalInvocationID.xy) + ivec2(u_copyRegion.xy);

	if (all(lessThan(coord.xy, ivec2(u_copyRegion.xy + u_copyRegion.zw)) ) )
	{
		vec4 diffuseValue = texelFetch(s_diffuseMap, coord.xy, 0);
		imageStore(s_copyImageUAV, coord, diffuseValue);
	}
}