#pragma once

#include <input/InputManager.h>
#include <input/InputDevice.h>
#include <system/HandleWrapper.h>
#include <cstdint>
#include <engine/RTTI.h>
#include <math/MathTypes.h>
#include <math/Uuid.h>
#include <memory>

#include <input/InputDevice.h>
#include <string_view>
#include <windowing/NativeWindow.h>

namespace hpl::input {

    enum class MouseButton : uint8_t { Left, Middle, Right, WheelUp, WheelDown, Button6, Button7, Button8, Button9, LastEnum };
    namespace details {
        static const std::string_view MouseButtonToString(MouseButton button);
        static const MouseButton StringToMouseButton(const std::string_view& button);
    }

    namespace internal::mouse {
        class InternalInputMouseHandle final : public HandleSharedWrapper {};

        InternalInputMouseHandle Initialize();
        cVector2l GetAbsPosition(InternalInputMouseHandle& handle);
        cVector2l GetRelPosition(InternalInputMouseHandle& handle);

        bool IsButtonPressed(InternalInputMouseHandle& handle, MouseButton button);

        window::internal::WindowInternalEvent::Handler& GetWindowEventHandle(InternalInputMouseHandle& handle); // internal use only
    } // namespace internal

    // wrapper over the internal implementation
    // this class is copyable and movable
    class InputMouseDevice final : public InputDevice{
        HPL_RTTI_IMPL_CLASS(InputDevice, InputMouseDevice, "{ac1b28f3-7a0f-4442-96bb-99b64adb5be6}")
    public:
        InputMouseDevice(internal::mouse::InternalInputMouseHandle&& handle) :
            m_impl(std::move(handle)) {
        }
        InputMouseDevice(const InputMouseDevice& other)
            : m_impl(other.m_impl) {
        }

        void operator=(InputMouseDevice& other) {
            m_impl = other.m_impl;
        }
        void operator=(InputMouseDevice&& other) {
            m_impl = std::move(other.m_impl);
        }

        ~InputMouseDevice() = default;

        /**
         * Check if a mouse button is down
         * \param eMouseButton the button to check
         * \return
         */
        bool ButtonIsDown(MouseButton button) {
            return internal::mouse::IsButtonPressed(m_impl, button);
        }
        /**
         * Get the absolute pos of the mouse.
         * \return
         */
        cVector2l GetAbsPosition() {
            return internal::mouse::GetAbsPosition(m_impl);
        }
        /**
         * Get the relative movement.
         * \return
         */
        cVector2l GetRelPosition() {
            return internal::mouse::GetRelPosition(m_impl);
        }

    private:
        internal::mouse::InternalInputMouseHandle m_impl;
    };
} // namespace hpl::input