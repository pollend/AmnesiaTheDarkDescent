#include <array>
#include <cstddef>
#include <cstdint>
#include <engine/UUID.h>

namespace hpl::UUID {

    bool operator==(const UUID& lhs, const UUID& rhs) {
        for(size_t i = 0; i < 16; i++) {
            if(lhs.m_data[i] != rhs.m_data[i]) {
                return false;
            }
        }
        return true;
    }


}