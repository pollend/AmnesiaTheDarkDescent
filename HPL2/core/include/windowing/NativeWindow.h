#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include <engine/Event.h>
#include <engine/RTTI.h>
#include <math/MathTypes.h>
#include <system/HandleWrapper.h>
#include <bx/debug.h>

union SDL_Event;

namespace hpl::window {

    enum class WindowEventType : uint16_t {
        ResizeWindowEvent,
        MoveWindowEvent,
        QuitEvent,
        // Mouse can move to a separate interface
        MouseMoveEvent,
        // keyboard can move to a separate interface

    };

    struct InternalEvent {
        union {
            SDL_Event* m_sdlEvent; // SDL_Event when compiling for SDL2
        };
    };

    struct WindowEventPayload {
        WindowEventType m_type;
        union {
            struct {
                uint32_t m_width;
                uint32_t m_height;
            } m_resizeWindow;
            struct {
                int32_t m_x;
                int32_t m_y;
            } m_moveWindow;
            struct {
                int32_t m_x;
                int32_t m_y;
            } m_mouseEvent;
        } payload;
    };
    using WindowEvent = hpl::Event<WindowEventPayload&>;

    enum class WindowType : uint8_t { Window, Fullscreen, Borderless };
    namespace internal {

        using WindowInternalEvent = hpl::Event<InternalEvent&>;
        class NativeWindowHandler final : public HandleWrapper {};

        void SetNativeWindowHandle(NativeWindowHandler& handler, WindowInternalEvent::Handler& eventHandle);
        void SetNativeDisplayHandle(NativeWindowHandler& handler, WindowEvent::Handler& eventHandle);

        NativeWindowHandler Initialize();
        void SetWindowTitle(NativeWindowHandler& handler, const std::string_view title);
        void SetWindowSize(NativeWindowHandler& handler, const cVector2l& size);

        void* NativeWindowHandle(NativeWindowHandler& handler);
        void* NativeDisplayHandle(NativeWindowHandler& handler);

        void Process(NativeWindowHandler& handler);
    } // namespace internal

    // wrapper over an opaque pointer to a windowing system
    // this is the only way to interact with the windowing system
    class NativeWindowWrapper final {
        HPL_RTTI_CLASS(NativeWindow, "{d17ea5c7-30f1-4d5d-b38e-1a7e88e137fc}")
    public:

        ~NativeWindowWrapper() {
        }
        NativeWindowWrapper() = default;
        NativeWindowWrapper(internal::NativeWindowHandler&& handle)
            : m_impl(std::move(handle)) {
        }
        NativeWindowWrapper(NativeWindowWrapper&& other)
            : m_impl(std::move(other.m_impl)) {
        }
        NativeWindowWrapper(const NativeWindowWrapper& other) = delete;

        NativeWindowWrapper& operator=(NativeWindowWrapper& other) = delete;
        void operator=(NativeWindowWrapper&& other) {
            m_impl = std::move(other.m_impl);
        }
        
        void* NativeWindowHandle() {
            BX_ASSERT(m_impl, "NativeWindowHandle is null")
            return internal::NativeWindowHandle(m_impl);
        }

        void* NativeDisplayHandle() {
            BX_ASSERT(m_impl, "NativeDisplayHandle is null")
            return internal::NativeDisplayHandle(m_impl);
        }

        void SetWindowSize(cVector2l size) {
            BX_ASSERT(m_impl, "NativeWindowHandle is null")
            internal::SetWindowSize(m_impl, size);
        }

        void SetWindowEventHandler(WindowEvent::Handler& handler) {
            BX_ASSERT(m_impl, "NativeWindowHandle is null")
            internal::SetNativeDisplayHandle(m_impl, handler);
        }

        void Process() {
            internal::Process(m_impl);
        }

        WindowType GetWindowType();
        cVector2l GetWindowSize();

    private:
        internal::NativeWindowHandler m_impl;
    };
} // namespace hpl::window