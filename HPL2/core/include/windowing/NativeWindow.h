#pragma once

#include "engine/Event.h"
#include <cstdint>
#include <math/MathTypes.h>
#include <engine/RTTI.h>
#include <memory>
#include <string_view>

union SDL_Event;

namespace hpl {
    // class WindowInterface;

    enum class WindowEventType : uint16_t{
        ResizeWindowEvent,
        MoveWindowEvent,
        QuitEvent,
        // Mouse can move to a separate interface
        MouseMoveEvent,

        // keyboard can move to a separate interface
        
    };

    struct InternalEvent {
        union {
            SDL_Event* m_sdlEvent;
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

    enum class WindowType : uint8_t {
        Window,
        Fullscreen,
        Borderless
    };

    class NativeWindow final {
        HPL_RTTI_CLASS(NativeWindow, "{d17ea5c7-30f1-4d5d-b38e-1a7e88e137fc}")
    public:
        using WindowInternalEvent = hpl::Event<InternalEvent&>;
        using WindowEvent = hpl::Event<WindowEventPayload&>;
        
        class Implementation {
            public:
                virtual ~Implementation() = default;

                virtual void* NativeWindowHandle() = 0;
                virtual void* NativeDisplayHandle() = 0;

                virtual void SetWindowInternalEventHandler(WindowInternalEvent::Handler& handler) = 0;
                virtual void SetWindowEventHandler(WindowEvent::Handler& handler) = 0;
                virtual void SetWindowTitle(const std::string_view title) = 0;
                virtual void SetWindowSize(const cVector2l& size) = 0;

                virtual void Process() = 0;
        };
        static Implementation* CreateWindow(); 

        ~NativeWindow() = default;
        NativeWindow();

        void* NativeWindowHandle();
        void* NativeDisplayHandle();

        void SetWindowSize(cVector2l size);
 
        void SetWindowInternalEventHandler(WindowInternalEvent::Handler& handler);
        void SetWindowEventHandler(WindowEvent::Handler& handler);
        void Process();

        WindowType GetWindowType();
        cVector2l GetWindowSize();
    private:
        std::unique_ptr<NativeWindow::Implementation> m_impl;
    };
}