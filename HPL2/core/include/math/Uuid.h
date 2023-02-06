#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace hpl {
    static constexpr size_t UUIDSize = 16;
    struct Uuid {
        uint8_t m_data[UUIDSize];
    };

    struct Guid {
        uint32_t m_data1;
        uint16_t m_data2;
        uint16_t m_data3;
        uint8_t m_data4[8];
    };
    bool operator==(const Uuid& lhs, const Uuid& rhs);
    static constexpr const Uuid NullUUID = Uuid{ 0 };

    namespace detail {

        // {C4F5C3C1-1B5C-4F5C-9F5C-1B5C4F5C9F5C}
        // C4F5C3C1-1B5C-4F5C-9F5C-1B5C4F5C9F5C
        // C4F5C3C11B5C4F5C9F5C1B5C4F5C9F5C
        constexpr Uuid From(const std::string_view str);
        constexpr Guid ToGUID(const Uuid& uuid);
        namespace Internal {
            constexpr char InvalidValue = 255;

            inline constexpr auto CharToHexDigit = []() {
                std::array<char, 256> result{};
                for (size_t i = 0; i < 256; ++i) {
                    result[i] = InvalidValue;
                }
                for (size_t i = 0; i < 10; ++i) {
                    result['0' + i] = i;
                }
                for (size_t i = 0; i < 6; ++i) {
                    result['A' + i] = 10 + i;
                    result['a' + i] = 10 + i;
                }
                return result;
            }();

            constexpr char GetValue(char c) {
                // Use a direct lookup table to convert from a valid ascii char to a 0-15 hex value
                return CharToHexDigit[c];
            }
        } // namespace Internal

        constexpr hpl::Uuid From(const std::string_view str) {
            auto current = str.begin();
            hpl::Uuid id;
            char c = *current;
            for (size_t i = 0; i < 16; i++) {
                if (c == '{' || c == '-' || c == '}') {
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

        constexpr hpl::Guid ToGUID(const hpl::Uuid& uuid) {
            union {
                uint32_t value;
                uint8_t cr[sizeof(uint32_t)];
            } data1 = { .cr = { uuid.m_data[0], uuid.m_data[1], uuid.m_data[2], uuid.m_data[3] } };

            union {
                uint16_t value;
                uint8_t cr[sizeof(uint16_t)];
            } data2 = { .cr = { uuid.m_data[4], uuid.m_data[5] } };

            union {
                uint16_t value;
                uint8_t cr[sizeof(uint16_t)];
            } data3 = { .cr = { uuid.m_data[6], uuid.m_data[7] } };

            return { .m_data1 = data1.value,
                     .m_data2 = data2.value,
                     .m_data3 = data3.value,
                     .m_data4 = { uuid.m_data[8],
                                  uuid.m_data[9],
                                  uuid.m_data[10],
                                  uuid.m_data[11],
                                  uuid.m_data[12],
                                  uuid.m_data[13],
                                  uuid.m_data[14],
                                  uuid.m_data[15] } };
        }
    } // namespace detail

} // namespace hpl
