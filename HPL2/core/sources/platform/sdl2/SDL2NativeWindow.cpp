#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <windowing/NativeWindow.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_video.h>
#include <math/MathTypes.h>

#include <mutex>
#include <queue>
#include <thread>

namespace hpl {
    enum WindowMessagePayloadType { 
        WindowMessage_None = 0,
        WindowMessage_Resize = 1,
        WindowMessage_SetTitle = 2
    };
    struct WindowMessagePayload {
        WindowMessagePayloadType m_type;
        union {
            struct {
                uint32_t width;
                uint32_t height;
            } m_resizeWindow;
            struct {
                char m_title[256];
            } m_setTitle;
        } m_payload;
    };

    class SDLWindow final : public NativeWindow::Implementation {
    public:
        SDLWindow();
        ~SDLWindow() override;

        virtual void* NativeWindowHandle() override;
        virtual void* NativeDisplayHandle() override;

        virtual void SetWindowInternalEventHandler(NativeWindow::WindowInternalEvent::Handler& handler) override;
        virtual void SetWindowEventHandler(NativeWindow::WindowEvent::Handler& handler) override;
        virtual void SetWindowTitle(const std::string_view title) override;
        virtual void SetWindowSize(const cVector2l& size) override;

        virtual void Process() override;

    private:
        void internalProcessMessagePayload(const WindowMessagePayload& event);
        void internalPushMessagePayload(const WindowMessagePayload& event);

        SDL_Window* m_window = nullptr;
        std::thread::id m_owningThread;
        std::mutex m_mutex;
        std::queue<WindowMessagePayload> m_processQueue;
        NativeWindow::WindowInternalEvent m_internalWindowEvent;
        NativeWindow::WindowEvent m_windowEvent;
    };

    SDLWindow::SDLWindow() {
        m_window = SDL_CreateWindow("HPL2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN);
        m_owningThread = std::this_thread::get_id();
    }

    void* SDLWindow::NativeWindowHandle() {
        SDL_SysWMinfo wmi;
        SDL_VERSION(&wmi.version);
        if (!SDL_GetWindowWMInfo(m_window, &wmi)) {
            return NULL;
        }

#if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
#if ENTRY_CONFIG_USE_WAYLAND
        wl_egl_window* win_impl = (wl_egl_window*)SDL_GetWindowData(_window, "wl_egl_window");
        if (!win_impl) {
            int width, height;
            SDL_GetWindowSize(_window, &width, &height);
            struct wl_surface* surface = wmi.info.wl.surface;
            if (!surface)
                return nullptr;
            win_impl = wl_egl_window_create(surface, width, height);
            SDL_SetWindowData(_window, "wl_egl_window", win_impl);
        }
        return (void*)(uintptr_t)win_impl;
#else
        return (void*)wmi.info.x11.window;
#endif
#elif BX_PLATFORM_OSX || BX_PLATFORM_IOS
        return wmi.info.cocoa.window;
#elif BX_PLATFORM_WINDOWS
        return wmi.info.win.window;
#elif BX_PLATFORM_ANDROID
        return wmi.info.android.window;
#endif // BX_PLATFORM_
    }

    void* SDLWindow::NativeDisplayHandle() {
        SDL_SysWMinfo wmi;
        SDL_VERSION(&wmi.version);
        if (!SDL_GetWindowWMInfo(m_window, &wmi)) {
            return NULL;
        }

#if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
#if ENTRY_CONFIG_USE_WAYLAND
        return wmi.info.wl.display;
#else
        return wmi.info.x11.display;
#endif // ENTRY_CONFIG_USE_WAYLAND
#else
        return nullptr;
#endif // BX_PLATFORM_*
    }

    void SDLWindow::SetWindowInternalEventHandler(NativeWindow::WindowInternalEvent::Handler& handler) {
        handler.Connect(m_internalWindowEvent);
    }

    void SDLWindow::SetWindowEventHandler(NativeWindow::WindowEvent::Handler& handler) {
        handler.Connect(m_windowEvent);
    }

    void SDLWindow::Process() {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            while (!m_processQueue.empty()) {
                auto& message = m_processQueue.front();
                internalProcessMessagePayload(message);
                m_processQueue.pop();
            }
        
        }

        SDL_Event event;
        InternalEvent internalEvent;
        WindowEventPayload windowEventPayload;
        while (SDL_PollEvent(&event)) {
            internalEvent.m_sdlEvent = &event;
            m_internalWindowEvent.Signal(internalEvent);
            switch (event.type) {
            case SDL_QUIT:
                windowEventPayload.m_type = WindowEventType::QuitEvent;
                m_windowEvent.Signal(windowEventPayload);
                break;
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                windowEventPayload.m_type = WindowEventType::ResizeWindowEvent;
                windowEventPayload.payload.m_resizeWindow.m_width = event.window.data1;
                windowEventPayload.payload.m_resizeWindow.m_height = event.window.data2;
                m_windowEvent.Signal(windowEventPayload);
                break;
            }
        }
    }


    void SDLWindow::internalProcessMessagePayload(const WindowMessagePayload& event) {
        switch (event.m_type) {
        case WindowMessagePayloadType::WindowMessage_SetTitle:
            SDL_SetWindowTitle(m_window, event.m_payload.m_setTitle.m_title);
            break;
        case WindowMessagePayloadType::WindowMessage_Resize:
            SDL_SetWindowSize(m_window, event.m_payload.m_resizeWindow.width, event.m_payload.m_resizeWindow.height);
            break;
        default:
            break;
        }
    }
    void SDLWindow::internalPushMessagePayload(const WindowMessagePayload& event) {
        if(m_owningThread == std::this_thread::get_id()) { // same thread, just process it
            internalProcessMessagePayload(event);
            return;
        }
        std::lock_guard<std::mutex> lk(m_mutex);
        m_processQueue.push(event);
    }

  void SDLWindow::SetWindowTitle(const std::string_view title) {
        WindowMessagePayload message;
        BX_ASSERT(title.size() < sizeof(message.m_payload.m_setTitle.m_title), "Title too long");
        std::memset(&message, 0, sizeof(WindowMessagePayload));
        message.m_type = WindowMessagePayloadType::WindowMessage_SetTitle;
        std::copy(title.begin(), title.end(), message.m_payload.m_setTitle.m_title);
        internalPushMessagePayload(message);
    }

    void SDLWindow::SetWindowSize(const cVector2l& size) {
        WindowMessagePayload message;
        std::memset(&message, 0, sizeof(WindowMessagePayload));
        message.m_type = WindowMessagePayloadType::WindowMessage_Resize;
        message.m_payload.m_resizeWindow.width = size.x;
        message.m_payload.m_resizeWindow.height = size.y;
        m_processQueue.push(message);
        internalPushMessagePayload(message);
    }


    // creates the implementation
    NativeWindow::Implementation* NativeWindow::CreateWindow() {
        return new SDLWindow();
    }
} // namespace hpl