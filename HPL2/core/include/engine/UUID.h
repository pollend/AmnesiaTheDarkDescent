#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace hpl {
    static constexpr size_t UUIDSize = 16;
    struct UUID {
        uint8_t m_data[UUIDSize];
    };

    struct GUID {
        uint32_t m_data1;
        uint16_t m_data2;
        uint16_t m_data3;
        uint8_t m_data4[8];
    };

    static constexpr const UUID NullUUID = UUID{0};

    // {C4F5C3C1-1B5C-4F5C-9F5C-1B5C4F5C9F5C}
    // C4F5C3C1-1B5C-4F5C-9F5C-1B5C4F5C9F5C
    // C4F5C3C11B5C4F5C9F5C1B5C4F5C9F5C
    constexpr UUID From(const std::string_view& str);
    constexpr GUID ToGUID(const UUID& uuid);
    constexpr bool operator==(const UUID& lhs, const UUID& rhs);

}





