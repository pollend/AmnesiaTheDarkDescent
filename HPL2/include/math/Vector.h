#pragma once

#include "Common_3/Utilities/Math/MathTypes.h"

namespace hpl::math {

    uint uint4ToUnorm8(const uint4& v);
    uint uint3ToUnorm8(const uint3& v);
    uint uint3ToUnorm8(const uint2& v);

    uint float4ToUnorm8(const float4& v);
    uint float3ToUnorm8(const float3& v);
    uint float2ToUnorm8(const float2& v);

    uint float2ToSnorm8(const float2& v);
    uint float3ToSnorm8(const float3& v);
    uint float4ToSnorm8(const float4& v);

    float2 snorm8ToFloat2(uint v);
    float3 snorm8ToFloat3(uint v);
    float4 snorm8ToFloat4(uint v);
}
