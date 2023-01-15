#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace hpl {
    static constexpr size_t GuidSize = 16;
    struct UUID {
        uint8_t m_data[GuidSize];
    };

    // {C4F5C3C1-1B5C-4F5C-9F5C-1B5C4F5C9F5C}
    // C4F5C3C1-1B5C-4F5C-9F5C-1B5C4F5C9F5C
    // C4F5C3C11B5C4F5C9F5C1B5C4F5C9F5C
    constexpr UUID From(const std::string_view& str);
    constexpr bool operator==(const UUID& lhs, const UUID& rhs);

}





