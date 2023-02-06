#pragma once

#include <input/InputChannel.h>

namespace hpl::input {

    class InputMouseDevice final : public InputChannel {
        HPL_RTTI_IMPL_CLASS(InputChannel, InputMouseDevice, "{ac1b28f3-7a0f-4442-96bb-99b64adb5be6}")
    public:
        InputMouseDevice(hpl::Guid deviceUID)
            : InputChannel(DeviceType::Mouse, deviceUID) {
        }
        ~InputMouseDevice() = default;

        
    };
} // namespace hpl::input