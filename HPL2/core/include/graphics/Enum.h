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

    enum class BlendOperator: uint8_t {
        Add,              //!< Src + Dest
        Subtract,         //!< Src - Dest
        ReverseSubtract,  //!< Dest - Src
        Min,              //!< min(Src, Dest)
        Max,              //!< max(Src, Dest)
    };

    enum class BlendOperand: uint8_t {
        None,
        Zero,           //!< 0, 0, 0, 0
        One,            //!< 1, 1, 1, 1
        SrcColor,       //!< Rs, Gs, Bs, As
        InvSrcColor,    //!< 1-Rs, 1-Gs, 1-Bs, 1-As
        SrcAlpha,       //!< As, As, As, As
        InvSrcAlpha,    //!< 1-As, 1-As, 1-As, 1-As
        DstAlpha,       //!< Ad, Ad, Ad, Ad
        InvDestAlpha,   //!< 1-Ad, 1-Ad, 1-Ad ,1-Ad
        DstColor,       //!< Rd, Gd, Bd, Ad
        InvDestColor,   //!< 1-Rd, 1-Gd, 1-Bd, 1-Ad
        AlphaSat,       //!< f, f, f, 1; f = min(As, 1-Ad)
        BlendFactor,    //!< Blend factor
        BlendInvFactor  //!< 1-Blend factor
    };

    enum class RTType: uint8_t {
        None,
        RT_Write,
        RT_WriteOnly,
        RT_MSAA_X2,
        RT_MSAA_X4,
        RT_MSAA_X8,
        RT_MSAA_X16,
    };

    enum class BlendFunc: uint32_t {
    };
    BlendFunc CreateBlendFunction(BlendOperator type, BlendOperand src, BlendOperand dst);
    BlendOperand GetBlendOperandSrc(BlendFunc func);
    BlendOperand GetBlendOperandDst(BlendFunc func);
    BlendOperator GetBlendOperator(BlendFunc func);

    Write operator|(Write lhs, Write rhs);
    bool operator&(Write lhs, Write rhs);

    ClearOp operator|(ClearOp lhs, ClearOp rhs);
    bool operator&(ClearOp lhs, ClearOp rhs);

} // namespace hpl