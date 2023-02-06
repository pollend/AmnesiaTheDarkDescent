#pragma once

#include <input/InputManagerInterface.h>
#include <windowing/WindowInterface.h>

namespace hpl::input {

    class InputManager : public InputManagerInterface {
    public:
        InputManager();
        ~InputManager() {
        }
    };
} // namespace hpl::input