#pragma once

#include <math/Uuid.h>
#include <eninge/RTTI.h>

namespace hpl::input {
    enum class DeviceType { 
        Keyboard, 
        Mouse, 
        Joystick
    };

    class InputChannel {
        HPL_RTTI_CLASS(InputChannel, "{9e04fa77-3045-4693-ba5d-be7d117a9122}")

    public:
        InputChannel(DeviceType deviceType, hpl::Guid deviceUID)
            : m_deviceType(deviceType)
            , m_deviceUID(deviceUID) {
        }
        virtual ~InputChannel() = default;

        DeviceType GetDeviceType() const {
            return m_deviceType;
        }

        hpl::Guid GetDeviceUID() const {
            return m_deviceUID;
        }

    private:
        DeviceType m_deviceType;
        hpl::Guid m_deviceUID;
    };
}; // namespace hpl::input


// template<>
// struct std::hash<hpl::input::InputChannel>
// {
//     std::size_t operator()(hpl::input::InputChannel const& s) const noexcept
//     {
//         std::size_t h1 = std::hash<std::string>{}(s.m_deviceUID.m_data1);
//         std::size_t h2 = std::hash<std::string>{}(s.last_name);
//         return h1 ^ (h2 << 1); // or use boost::hash_combine
//     }
// };