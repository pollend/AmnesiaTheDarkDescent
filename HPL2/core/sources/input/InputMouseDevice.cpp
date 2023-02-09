#include <input/InputMouseDevice.h>

namespace hpl::input {
    namespace details {
        const std::string_view MouseButtonToString(MouseButton button) {
            switch (button) {
            case MouseButton::Left:
                return "LeftMouse";
            case MouseButton::Middle:
                return "MiddleMouse";
            case MouseButton::Right:
                return "RightMouse";
            case MouseButton::WheelUp:
                return "WheelUp";
            case MouseButton::WheelDown:
                return "WheelDown";
            case MouseButton::Button6:
                return "Mouse6";
            case MouseButton::Button7:
                return "Mouse7";
            case MouseButton::Button8:
                return "Mouse8";
            case MouseButton::Button9:
                return "Mouse9";
            default:
                break;
            }
            return "Unknown";
        }

        const MouseButton StringToMouseButton(const std::string_view& button) {
            if (button == "LeftMouse") {
                return MouseButton::Left;
            }
            if (button == "MiddleMouse") {
                return MouseButton::Middle;
            }
            if (button == "RightMouse") {
                return MouseButton::Right;
            }
            if (button == "WheelUp") {
                return MouseButton::WheelUp;
            }
            if (button == "WheelDown") {
                return MouseButton::WheelDown;
            }
            if (button == "Mouse6") {
                return MouseButton::Button6;
            }
            if (button == "Mouse7") {
                return MouseButton::Button7;
            }
            if (button == "Mouse8") {
                return MouseButton::Button8;
            }
            if (button == "Mouse9") {
                return MouseButton::Button9;
            }
            return MouseButton::LastEnum;
        }

    } // namespace details
} // namespace hpl::input