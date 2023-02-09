#include <input/InputMouseDevice.h>

#include "math/MathTypes.h"
#include "windowing/NativeWindow.h"
#include <input/InputMouseDevice.h>
#include <memory>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mouse.h>

namespace hpl::input::internal {
   struct NativeInputImpl {
        window::internal::WindowInternalEvent::Handler m_windowEventHandle;
        cVector2l m_mousePosition;
        cVector2l m_mouseRelPosition;
        std::array<bool, static_cast<size_t>(MouseButton::LastEnum)> m_mouseButton;
   };

    InternalInputMouseHandle Initialize() {
        InternalInputMouseHandle handle = InternalInputMouseHandle {
            InternalInputMouseHandle::Ptr(new NativeInputImpl(), [](void* ptr) {
                auto impl = static_cast<NativeInputImpl*>(ptr);
                impl->m_windowEventHandle.Disconnect();
                delete impl;
            })
        };

        auto* impl = static_cast<NativeInputImpl*>(handle.Get());
        impl->m_windowEventHandle = window::internal::WindowInternalEvent::Handler([impl](hpl::window::InternalEvent& event) {
            auto& sdlEvent = *event.m_sdlEvent;

            switch (sdlEvent.type) {
            case SDL_EventType::SDL_MOUSEMOTION:
                impl->m_mousePosition = cVector2l(sdlEvent.motion.x, sdlEvent.motion.y);
                impl->m_mouseRelPosition = cVector2l(sdlEvent.motion.xrel, sdlEvent.motion.yrel);
                break;
            case SDL_EventType::SDL_MOUSEBUTTONDOWN:
            case SDL_EventType::SDL_MOUSEBUTTONUP:{
                const bool isPressed = sdlEvent.button.state == SDL_PRESSED;
                switch (sdlEvent.button.button) {
                    case SDL_BUTTON_LEFT:
                        impl->m_mouseButton[static_cast<size_t>(MouseButton::Left)] = isPressed;
                        break;
                    case SDL_BUTTON_MIDDLE:
                        impl->m_mouseButton[static_cast<size_t>(MouseButton::Middle)] = isPressed;
                        break;
                    case SDL_BUTTON_RIGHT:
                        impl->m_mouseButton[static_cast<size_t>(MouseButton::Right)] = isPressed;
                        break;
                    case SDL_BUTTON_X1:
                        impl->m_mouseButton[static_cast<size_t>(MouseButton::Button6)] = isPressed;
                        break;
                    case SDL_BUTTON_X2:
                        impl->m_mouseButton[static_cast<size_t>(MouseButton::Button7)] = isPressed;
                        break;
                }
                break;
            }
            case SDL_EventType::SDL_MOUSEWHEEL:
                if (sdlEvent.wheel.y > 0) {
                    impl->m_mouseButton[static_cast<size_t>(MouseButton::WheelUp)] = true;
                    impl->m_mouseButton[static_cast<size_t>(MouseButton::WheelDown)] = false;
                } else if (sdlEvent.wheel.y < 0) {
                    impl->m_mouseButton[static_cast<size_t>(MouseButton::WheelUp)] = false;
                    impl->m_mouseButton[static_cast<size_t>(MouseButton::WheelDown)] = true;
                }
                break;
            default:
                break;
            }
        });

        return handle;
    }
    cVector2l GetAbsPosition(InternalInputMouseHandle& handle) {
        auto* impl = static_cast<NativeInputImpl*>(handle.Get());
        return impl->m_mousePosition;
    }

    cVector2l GetRelPosition(InternalInputMouseHandle& handle) {
        auto* impl = static_cast<NativeInputImpl*>(handle.Get());
        return impl->m_mouseRelPosition;
    }

    window::internal::WindowInternalEvent::Handler& GetWindowEventHandle(InternalInputMouseHandle& handle) {
        auto* impl = static_cast<NativeInputImpl*>(handle.Get());
        return impl->m_windowEventHandle;
    }

}