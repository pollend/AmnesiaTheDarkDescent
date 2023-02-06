#include <sdl/SDLWindow.h>

#include <platform/sdl2/SDLWindow.h>
#include <math/MathTypes.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_video.h>
#include <mutex>
#include <queue>
#include <thread>

namespace hpl {
    enum class WindowMessagePayloadType {
        Resize
    };
    struct WindowMessagePayload {
        WindowMessagePayloadType m_type;
        union {
            struct {
                uint32_t width;
                uint32_t height;
            } m_resizeWindow;
        } m_payload;
    };

    struct SDLWindow::Impl {
    public:
        SDL_Window* m_window = nullptr;
        std::thread::id m_owningThread;
        std::mutex m_mutex;
        std::queue<WindowMessagePayload> m_processQueue;
    };

    SDLWindow::SDLWindow()
        : m_impl(std::make_unique<Impl>()) {
        m_impl->m_window = SDL_CreateWindow("HPL2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN);
        m_impl->m_owningThread = std::this_thread::get_id();
    }
    SDLWindow::~SDLWindow() {
    }

    void* SDLWindow::NativeWindowHandle() {
        SDL_SysWMinfo wmi;
        SDL_VERSION(&wmi.version);
        if (!SDL_GetWindowWMInfo(m_impl->m_window, &wmi)) {
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
		if (!SDL_GetWindowWMInfo(m_impl->m_window, &wmi) )
		{
			return NULL;
		}

#	if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
#		if ENTRY_CONFIG_USE_WAYLAND
		return wmi.info.wl.display;
#		else
		return wmi.info.x11.display;
#		endif // ENTRY_CONFIG_USE_WAYLAND
#	else
		return nullptr;
#	endif // BX_PLATFORM_*
    }

    void SDLWindow::SetWindowInternalEventHandler(WindowInternalEvent::Handler& handler) {
        handler.Connect(m_internalWindowEvent);
    }

    void SDLWindow::SetWindowEventHandler(WindowEvent::Handler& handler) {
        handler.Connect(m_windowEvent);
    }

    void SDLWindow::SetWindowSize(cVector2l size) {
        if(m_impl->m_owningThread == std::this_thread::get_id()) {
            SDL_SetWindowSize(m_impl->m_window, size.x, size.y);
        }
        std::lock_guard<std::mutex> lk(m_impl->m_mutex);
        WindowMessagePayload message;
        message.m_type = WindowMessagePayloadType::Resize;
        message.m_payload.m_resizeWindow.width = size.x;
        message.m_payload.m_resizeWindow.height = size.y; 
        m_impl->m_processQueue.push(message);
    }

    void SDLWindow::Process() {
        {
            std::lock_guard<std::mutex> processLock(m_impl->m_mutex);
            if(!m_impl->m_processQueue.empty()) {
                while(!m_impl->m_processQueue.empty()) {
                    auto& message = m_impl->m_processQueue.front();
                    switch(message.m_type) {
                        case WindowMessagePayloadType::Resize:
                            SDL_SetWindowSize(m_impl->m_window, message.m_payload.m_resizeWindow.width, message.m_payload.m_resizeWindow.height);
                            break;
                    }
                    m_impl->m_processQueue.pop();
                }
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
                
            }
        }
    }

    WindowType SDLWindow::GetWindowType() {
        return WindowType::Window;
    }
    cVector2l SDLWindow::GetWindowSize() {
        return cVector2l(0, 0);
    }
} // namespace hpl