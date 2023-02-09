#pragma once

#include <engine/RTTI.h>
#include <math/MathTypes.h>
#include <math/Uuid.h>
#include <memory>

#include <input/InputDevice.h>
#include <windowing/NativeWindow.h>

namespace hpl::input {

    enum class MouseButton { Unknown, Left, Middle, Right, WheelUp, WheelDown, Button6, Button7, Button8, Button9 };

    namespace internal {
        class InputMouseHandle final : public HandleWrapper {};

        InputMouseHandle Initialize();
        cVector2l GetAbsPosition(InputMouseHandle& handle);
        cVector2l GetRelPosition(InputMouseHandle& handle);
		
		window::internal::WindowInternalEvent::Handler& GetWindowEventHandle(InputMouseHandle& handle); // internal use only

    } // namespace internal

    class InputMouseDevice final {
        HPL_RTTI_CLASS(InputMouseDevice, "{ac1b28f3-7a0f-4442-96bb-99b64adb5be6}")
    public:
        InputMouseDevice(internal::InputMouseHandle&& handle)
            : m_impl(std::move(handle)) {
        }

        // static Implementation* CreateInputDevice();

        ~InputMouseDevice() = default;

        /**
         * Check if a mouse button is down
         * \param eMouseButton the button to check
         * \return
         */
        bool ButtonIsDown(MouseButton) {
            return false;
        }
        /**
         * Get the absolute pos of the mouse.
         * \return
         */
        cVector2l GetAbsPosition() {
            return cVector2l(0, 0);
        }
        /**
         * Get the relative movement.
         * \return
         */
        cVector2l GetRelPosition() {
            return cVector2l(0, 0);
        }
        // /**
        //  * \param eMouseButton The button to change to string.
        //  * \return The name of the button as a string.
        //  */
        // tString ButtonToString(MouseButton);
        // /**
        //  * \param tString Name of the button
        //  * \return enum of the button.
        //  */
        // MouseButton StringToButton(const tString&);
    private:
        internal::InputMouseHandle m_impl;
    };
} // namespace hpl::input