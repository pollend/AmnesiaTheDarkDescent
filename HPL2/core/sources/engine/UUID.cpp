#include <array>
#include <cstddef>
#include <cstdint>
#include <engine/UUID.h>

namespace hpl {

    namespace Internal {
        constexpr char InvalidValue = 255;

        inline constexpr auto CharToHexDigit = []() {
            std::array<char, 256> result{};
            for(size_t i = 0; i < 256; ++i) {
                result[i] = InvalidValue;
            }
            for(size_t i = 0; i < 10; ++i) {
                result['0' + i] = i;
            }
            for(size_t i = 0; i < 6; ++i) {
                result['A' + i] = 10 + i;
                result['a' + i] = 10 + i;
            }
            return result;
        }();

        constexpr char GetValue(char c)
        {
            // Use a direct lookup table to convert from a valid ascii char to a 0-15 hex value
            return CharToHexDigit[c];
        }
    }

    constexpr bool operator==(const UUID& lhs, const UUID& rhs) {
        for(size_t i = 0; i < 16; i++) {
            if(lhs.m_data[i] != rhs.m_data[i]) {
                return false;
            }
        }
        return true;
    }
        
    constexpr GUID ToGUID(const UUID& uuid) {
        union {
            uint32_t value;
            uint8_t cr[sizeof(uint32_t)];
        } data1 = {
            .cr = {uuid.m_data[0], uuid.m_data[1], uuid.m_data[2], uuid.m_data[3]}
        };

        union {
            uint16_t value;
            uint8_t cr[sizeof(uint16_t)];
        } data2 = {.cr = {uuid.m_data[4], uuid.m_data[5]}};

        union {
            uint16_t value;
            uint8_t cr[sizeof(uint16_t)];
        } data3 = {.cr = {uuid.m_data[6], uuid.m_data[7]}};

        return {
            .m_data1 = data1.value,
            .m_data2 = data2.value,
            .m_data3 = data3.value,
            .m_data4 = {uuid.m_data[8], uuid.m_data[9], uuid.m_data[10], uuid.m_data[11], uuid.m_data[12], uuid.m_data[13], uuid.m_data[14], uuid.m_data[15]}
        };
    }

    constexpr UUID From(const std::string_view& str) {
       auto current = str.begin();
        hpl::UUID id;
        char c = *current;
        for(size_t i = 0; i < 16; i++) { 
            if(c == '{' || c == '-' || c == '}') {
                current++;
            }
            
            c = *(current++);

            id.m_data[i] = Internal::GetValue(c);
            
            c = *(current++);
            
            id.m_data[i] <<= 4;
            id.m_data[i] |= Internal::GetValue(c);
        }
        return id;
    }

}