#include "math/Vector.h"

namespace hpl::math {


    uint uint4ToUnorm8(const uint4& v) {
        return (v.x & 0xff) | ((v.y & 0xff) << 8) | ((v.z & 0xff) << 16) | ((v.w & 0xff) << 24);
    }

    uint uint3ToUnorm8(const uint3& v) {
        return (v.x & 0xff) | ((v.y & 0xff) << 8) | ((v.z & 0xff) << 16);
    }

    uint uint3ToUnorm8(const uint2& v) {
        return (v.x & 0xff) | ((v.y & 0xff) << 8);
    }

    uint float4ToUnorm8(const float4& v) {
        int x = int(v.x * 255.0f);
        int y = int(v.y * 255.0f);
        int z = int(v.z * 255.0f);
        int w = int(v.w * 255.0f);
        return (x & 0xff) | ((y & 0xff) << 8) | ((z & 0xff) << 16) | ((w & 0xff) << 24);
    }

    uint float3ToUnorm8(const float3& v) {
        int x = int(v.x * 255.0f);
        int y = int(v.y * 255.0f);
        int z = int(v.z * 255.0f);
        return (x & 0xff) | ((y & 0xff) << 8) | ((z & 0xff) << 16);
    }

    uint float2ToUnorm8(const float2& v) {
        int x = int(v.x * 255.0f);
        int y = int(v.y * 255.0f);
        return (x & 0xff) | ((y & 0xff) << 8);
    }

    uint float2ToSnorm8(const float2& v)
    {
        float scale = 127.0f / sqrtf(v.x * v.x + v.y * v.y);
        int x = int(v.x * scale);
        int y = int(v.y * scale);
        return (x & 0xff) | ((y & 0xff) << 8);
    }

    uint float3ToSnorm8(const float3& v)
    {
        float scale = 127.0f / sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
        int x = int(v.x * scale);
        int y = int(v.y * scale);
        int z = int(v.z * scale);
        return (x & 0xff) | ((y & 0xff) << 8) | ((z & 0xff) << 16);
    }

    uint float4ToSnorm8(const float4& v)
    {
        float scale = 127.0f / sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
        int x = int(v.x * scale);
        int y = int(v.y * scale);
        int z = int(v.z * scale);
        int w = int(v.w * scale);
        return (x & 0xff) | ((y & 0xff) << 8) | ((z & 0xff) << 16) | ((w & 0xff) << 24);
    }

    float2 snorm8ToFloat2(uint v)
    {
        float x = static_cast<signed char>(v & 0xff);
        float y = static_cast<signed char>((v >> 8) & 0xff);
        return max(float2(x, y) / 127.0f, float2(-1.f));
    }

    float3 snorm8ToFloat3(uint v)
    {
        float x = static_cast<signed char>(v & 0xff);
        float y = static_cast<signed char>((v >> 8) & 0xff);
        float z = static_cast<signed char>((v >> 16) & 0xff);
        return max(float3(x, y, z) / 127.0f, float3(-1.f));
    }

    float4 snorm8ToFloat4(uint v)
    {
        float x = static_cast<signed char>(v & 0xff);
        float y = static_cast<signed char>((v >> 8) & 0xff);
        float z = static_cast<signed char>((v >> 16) & 0xff);
        float w = static_cast<signed char>((v >> 24) & 0xff);
        return max(float4(x, y, z, w) / 127.0f, float4(-1.f));
    }

}
