#pragma once

#include <cstdint>
namespace hpl
{
    enum class DepthTest: uint8_t {
        None,
        Less,
        LessEqual,
        Equal,
        GreaterEqual,
        Greater,
        NotEqual,
        Always,
    };

    enum class Write: uint32_t {
        None = 0,
        R = 1 << 0,
        G = 1 << 1,
        B = 1 << 2,
        A = 1 << 3,
        Depth = 1 << 4,
        RGB = R | G | B,
        RGBA = RGB | A,
    };

    enum class ClearOp: uint32_t {
        None = 0,
        Color = 1 << 0,
        Depth = 1 << 1,
        Stencil = 1 << 2,
        All = Color | Depth | Stencil,
    };

    enum class Cull: uint8_t {
        None,
        Clockwise,
        CounterClockwise,
    };

    Write operator|(Write lhs, Write rhs);
    bool operator&(Write lhs, Write rhs);

    ClearOp operator|(ClearOp lhs, ClearOp rhs);
    bool operator&(ClearOp lhs, ClearOp rhs);

} // namespace hpl