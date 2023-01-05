#include <graphics/Enum.h>

namespace hpl {

    static const uint32_t BlendSrcMask = 0x0F;
    static const uint32_t BlendDstMask = 0x0F;
    static const uint32_t BlendOpMask =  0x0F;

    static const uint32_t BlendSrcShift = 8;
    static const uint32_t BlendDstShift = 4;
    static const uint32_t BlendOpShift = 0;

    
    Write operator|(Write lhs, Write rhs) {
        return static_cast<Write>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    } 

    bool operator&(Write lhs, Write rhs) {
        return static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs);
    }

    ClearOp operator|(ClearOp lhs, ClearOp rhs) {
        return static_cast<ClearOp>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    }

    bool operator&(ClearOp lhs, ClearOp rhs) {
        return static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs);
    }

    BlendFunc CreateBlendFunction(BlendOperator type, BlendOperand src, BlendOperand dst) {
        return static_cast<BlendFunc>(((static_cast<uint32_t>(type) & BlendOpMask) << BlendOpShift) | ((static_cast<uint32_t>(src) & BlendSrcMask) << BlendSrcShift) | ((static_cast<uint32_t>(dst) & BlendDstMask) << BlendDstShift));
    }

    BlendOperand GetBlendOperandSrc(BlendFunc func) {
        return static_cast<BlendOperand>((static_cast<uint32_t>(func) >> BlendSrcShift) & BlendSrcMask);
    }
    
    BlendOperand GetBlendOperandDst(BlendFunc func) {
        return static_cast<BlendOperand>((static_cast<uint32_t>(func) >> BlendDstShift) & BlendDstMask);
    }

    BlendOperator GetBlendOperator(BlendFunc func) {
        return static_cast<BlendOperator>((static_cast<uint32_t>(func) >> BlendOpShift) & BlendOpMask);
    }

}