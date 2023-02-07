#pragma once

#include <input/InputDeviceId.h>
#include <input/InputManagerInterface.h>
#include <memory>
#include <vector>
#include <windowing/NativeWindow.h>

namespace hpl::input {
    class InputDevice;
    class InputManager;


    class InputManager : public InputManagerInterface {
    public:

        static constexpr InputDeviceID MouseDevice = InputDeviceID("Mouse");

        struct InputEntry {
            InputDeviceID m_id;
            std::shared_ptr<InputDevice> m_inputDevice;
        };
        
        InputManager();
        ~InputManager() {
        }
    private:
        std::vector<InputEntry> m_inputsDevices;
    };
} // namespace hpl::input