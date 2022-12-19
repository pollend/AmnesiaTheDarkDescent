#include <graphics/Enum.h>

namespace hpl {
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
}