#include <cstddef>
#include <memory>
#include <windowing/NativeWindow.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <mutex>
#include <queue>
#include <thread>

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_video.h>
#include <math/MathTypes.h>


namespace hpl::window::internal {
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

    struct NativeWindowImpl {
        SDL_Window* m_window = nullptr;
        std::thread::id m_owningThread;
        std::mutex m_mutex;
        std::queue<WindowMessagePayload> m_processQueue;
        internal::WindowInternalEvent m_internalWindowEvent;
        hpl::window::WindowEvent m_windowEvent;
    };

    void internalProcessMessagePayload(NativeWindowImpl& impl,const WindowMessagePayload& event) {
        switch (event.m_type) {
        case WindowMessagePayloadType::WindowMessage_SetTitle:
            SDL_SetWindowTitle(impl.m_window, event.m_payload.m_setTitle.m_title);
            break;
        case WindowMessagePayloadType::WindowMessage_Resize:
            SDL_SetWindowSize(impl.m_window, event.m_payload.m_resizeWindow.width, event.m_payload.m_resizeWindow.height);
            break;
        default:
            break;
        }
    }
    void internalPushMessagePayload(NativeWindowImpl& impl, const WindowMessagePayload& event) {
        if(impl.m_owningThread == std::this_thread::get_id()) { // same thread, just process it
            internalProcessMessagePayload(impl, event);
            return;
        }
        std::lock_guard<std::mutex> lk(impl.m_mutex);
        impl.m_processQueue.push(event);
    }


    NativeWindowHandler Initialize() {
        NativeWindowHandler handle = { NativeWindowHandler::Ptr(new NativeWindowImpl(), [](void* ptr) {
            auto* impl = static_cast<NativeWindowImpl*>(ptr);
            impl->m_internalWindowEvent.DisconnectAllHandlers();
            impl->m_windowEvent.DisconnectAllHandlers();
            SDL_DestroyWindow(impl->m_window);
            delete impl;
        }) };

        auto impl = static_cast<NativeWindowImpl*>(handle.Get());
        impl->m_owningThread = std::this_thread::get_id();
        impl->m_window = SDL_CreateWindow("HPL2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        

        return handle;
    }
    void SetWindowInternalEventHandler(NativeWindowHandler& handler, WindowInternalEvent::Handler& eventHandle) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        eventHandle.Connect(impl->m_internalWindowEvent);
    }
    void SetWindowEventHandler(NativeWindowHandler& handler, WindowEvent::Handler& eventHandle) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        eventHandle.Connect(impl->m_windowEvent);
    }
            
    void* NativeWindowHandle(NativeWindowHandler& handler) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        
        SDL_SysWMinfo wmi;
        SDL_VERSION(&wmi.version);
        if (!SDL_GetWindowWMInfo(impl->m_window, &wmi)) {
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

    void* NativeDisplayHandle(NativeWindowHandler& handler) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        
        SDL_SysWMinfo wmi;
        SDL_VERSION(&wmi.version);
        if (!SDL_GetWindowWMInfo(impl->m_window, &wmi)) {
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


    void SetWindowTitle(NativeWindowHandler& handler, const std::string_view title) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        WindowMessagePayload message;
        BX_ASSERT(title.size() < sizeof(message.m_payload.m_setTitle.m_title), "Title too long");
        std::memset(&message, 0, sizeof(WindowMessagePayload));
        message.m_type = WindowMessagePayloadType::WindowMessage_SetTitle;
        std::copy(title.begin(), title.end(), message.m_payload.m_setTitle.m_title);
        internalPushMessagePayload(*impl, message);
    }
    void SetWindowSize(NativeWindowHandler& handler, const cVector2l& size) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        WindowMessagePayload message;
        std::memset(&message, 0, sizeof(WindowMessagePayload));
        message.m_type = WindowMessagePayloadType::WindowMessage_Resize;
        message.m_payload.m_resizeWindow.width = size.x;
        message.m_payload.m_resizeWindow.height = size.y;
        internalPushMessagePayload(*impl, message);
    }

    void Process(NativeWindowHandler& handler) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        {
            std::lock_guard<std::mutex> lk(impl->m_mutex);
            while (!impl->m_processQueue.empty()) {
                auto& message = impl->m_processQueue.front();
                internalProcessMessagePayload(*impl, message);
                impl->m_processQueue.pop();
            }
        
        }

        SDL_Event event;
        InternalEvent internalEvent;
        WindowEventPayload windowEventPayload;
        while (SDL_PollEvent(&event)) {
            internalEvent.m_sdlEvent = &event;
            impl->m_internalWindowEvent.Signal(internalEvent);
            switch (event.type) {
            case SDL_QUIT:
                windowEventPayload.m_type = WindowEventType::QuitEvent;
                impl->m_windowEvent.Signal(windowEventPayload);
                break;
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                windowEventPayload.m_type = WindowEventType::ResizeWindowEvent;
                windowEventPayload.payload.m_resizeWindow.m_width = event.window.data1;
                windowEventPayload.payload.m_resizeWindow.m_height = event.window.data2;
                impl->m_windowEvent.Signal(windowEventPayload);
                break;
            }
        }
    }
}