/**
* Copyright 2023 Michael Pollind
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* 
*     http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "engine/Interface.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>
#include <windowing/NativeWindow.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_video.h>
#include <math/MathTypes.h>

#include <engine/IUpdateEventLoop.h>

#include <SDL_mouse.h>
#include <SDL_stdinc.h>
namespace hpl::window::internal {

    struct NativeWindowImpl {
        SDL_Window* m_window = nullptr;
        std::thread::id m_owningThread;
        std::recursive_mutex m_mutex;
        std::vector<std::function<void(NativeWindowImpl&)>> m_processCmd;
        internal::WindowInternalEvent m_internalWindowEvent;
        hpl::window::WindowEvent m_windowEvent;
        cVector2l m_windowSize;
        uint32_t m_windowFlags = 0;
    };

    void InternalHandleCmd(NativeWindowImpl& impl, std::function<void(NativeWindowImpl&)> handle) {
        if (impl.m_owningThread == std::this_thread::get_id()) { // same thread, just process it
            handle(impl);
            return;
        }
        std::lock_guard<std::recursive_mutex> lk(impl.m_mutex);
        impl.m_processCmd.push_back(handle);
    }

    NativeWindowHandler Initialize(const WindowStyle& style) {
        auto ptr = NativeWindowHandler::Ptr(new NativeWindowImpl(), [](void* ptr) {
            auto* impl = static_cast<NativeWindowImpl*>(ptr);
            impl->m_internalWindowEvent.DisconnectAllHandlers();
            impl->m_windowEvent.DisconnectAllHandlers();
            SDL_DestroyWindow(impl->m_window);
            SDL_Quit();
            delete impl;
        });
        uint32_t flags = 
            (any(style & WindowStyle::WindowStyleTitleBar) ? SDL_WINDOW_BORDERLESS : 0) |
            (any(style & WindowStyle::WindowStyleResizable) ? SDL_WINDOW_RESIZABLE : 0) |
            (any(style & WindowStyle::WindowStyleBorderless) ? SDL_WINDOW_BORDERLESS : 0) |
            (any(style & WindowStyle::WindowStyleFullscreen) ? SDL_WINDOW_FULLSCREEN : 0) |
            (any(style & WindowStyle::WindowStyleFullscreenDesktop) ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) |
            (any(style & WindowStyle::WindowStyleClosable) ? SDL_WINDOW_ALLOW_HIGHDPI : 0) |
            (any(style & WindowStyle::WindowStyleMinimizable) ? SDL_WINDOW_MINIMIZED : 0) |
            (any(style & WindowStyle::WindowStyleMaximizable) ? SDL_WINDOW_MAXIMIZED : 0) |
            SDL_WINDOW_SHOWN;

        NativeWindowHandler handle = NativeWindowHandler(std::move(ptr));

        auto impl = static_cast<NativeWindowImpl*>(handle.Get());
        impl->m_owningThread = std::this_thread::get_id();
        impl->m_window = SDL_CreateWindow("HPL2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, flags);
        impl->m_windowSize = cVector2l(1280, 720);

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
        BX_ASSERT(impl->m_window, "Window is not initialized");

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
        BX_ASSERT(impl->m_window, "Window is not initialized");

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

    cVector2l GetWindowSize(NativeWindowHandler& handler) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        BX_ASSERT(impl->m_window, "Window is not initialized");
        return impl->m_windowSize;
    }

    void ConnectInternalEventHandler(NativeWindowHandler& handler, WindowInternalEvent::Handler& eventHandle) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        eventHandle.Connect(impl->m_internalWindowEvent);
    }

    void ConnectionWindowEventHandler(NativeWindowHandler& handler, WindowEvent::Handler& eventHandle) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        eventHandle.Connect(impl->m_windowEvent);
    }

    WindowInternalEvent& NativeInternalEvent(NativeWindowHandler& handler) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        return impl->m_internalWindowEvent;
    }

    WindowEvent& NativeWindowEvent(NativeWindowHandler& handler) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        return impl->m_windowEvent;
    }

    void SetWindowTitle(NativeWindowHandler& handler, const std::string_view title) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());

        std::string name(title);
        InternalHandleCmd(*impl, [name](NativeWindowImpl& impl) {
            SDL_SetWindowTitle(impl.m_window, name.c_str());
        });
    }

    void SetWindowSize(NativeWindowHandler& handler, const cVector2l& size) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        InternalHandleCmd(*impl, [size](NativeWindowImpl& impl) {
            SDL_SetWindowSize(impl.m_window, size.x, size.y);
        });
    }

    void WindowGrabCursor(NativeWindowHandler& handler) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        InternalHandleCmd(*impl, [](NativeWindowImpl& impl) {
            SDL_SetWindowGrab(impl.m_window, SDL_TRUE);
        });
    }
    void WindowReleaseCursor(NativeWindowHandler& handler) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        InternalHandleCmd(*impl, [](NativeWindowImpl& impl) {
            SDL_SetWindowGrab(impl.m_window, SDL_FALSE);
        });
    }
    void ShowHardwareCursor(NativeWindowHandler& handler) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        InternalHandleCmd(*impl, [](NativeWindowImpl& impl) {
            SDL_ShowCursor(SDL_ENABLE);
        });
    }
    void HideHardwareCursor(NativeWindowHandler& handler) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        InternalHandleCmd(*impl, [](NativeWindowImpl& impl) {
            SDL_ShowCursor(SDL_DISABLE);
        });
    }
    void ConstrainCursor(NativeWindowHandler& handler) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        InternalHandleCmd(*impl, [](NativeWindowImpl& impl) {
            SDL_SetRelativeMouseMode(SDL_TRUE);
        });
    }
    void UnconstrainCursor(NativeWindowHandler& handler) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        InternalHandleCmd(*impl, [](NativeWindowImpl& impl) {
            SDL_SetRelativeMouseMode(SDL_FALSE);
        });
    }

    WindowStatus GetWindowStatus(NativeWindowHandler& handler) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());
        WindowStatus status =
            (((impl->m_windowFlags & SDL_WINDOW_INPUT_FOCUS) > 0 ? WindowStatus::WindowStatusInputFocus : WindowStatus::WindowStatusNone) |
             ((impl->m_windowFlags & SDL_WINDOW_MOUSE_FOCUS) > 0 ? WindowStatus::WindowStatusInputMouseFocus
                                                                 : WindowStatus::WindowStatusNone) |
             ((impl->m_windowFlags & SDL_WINDOW_MINIMIZED) > 0 ? WindowStatus::WindowStatusWindowMinimized
                                                               : WindowStatus::WindowStatusNone) |
             ((impl->m_windowFlags & SDL_WINDOW_SHOWN) > 0 ? WindowStatus::WindowStatusVisible : WindowStatus::WindowStatusNone));
        return status;
    }

    void Process(NativeWindowHandler& handler) {
        auto impl = static_cast<NativeWindowImpl*>(handler.Get());

        std::lock_guard<std::recursive_mutex> lk(impl->m_mutex);
        for (auto& handler : impl->m_processCmd) {
            handler(*impl);
        }
        impl->m_processCmd.clear();

        impl->m_windowFlags = SDL_GetWindowFlags(impl->m_window);
        InternalEvent internalEvent;
        WindowEventPayload windowEventPayload;
        while (SDL_PollEvent(&internalEvent.m_sdlEvent)) {
            auto& event = internalEvent.m_sdlEvent;
            impl->m_internalWindowEvent.Signal(internalEvent);
            switch (event.type) {
            case SDL_QUIT:
                windowEventPayload.m_type = WindowEventType::QuitEvent;
                impl->m_windowEvent.Signal(windowEventPayload);
                break;
            case SDL_WINDOWEVENT:
                {
                    switch (event.window.event) {
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        windowEventPayload.m_type = WindowEventType::ResizeWindowEvent;
                        windowEventPayload.payload.m_resizeWindow.m_width = event.window.data1;
                        windowEventPayload.payload.m_resizeWindow.m_height = event.window.data2;
                        impl->m_windowSize = cVector2l(event.window.data1, event.window.data2);
                        impl->m_windowEvent.Signal(windowEventPayload);
                        break;
                    default:
                        break;
                    }
                }
            }
        }
    }
} // namespace hpl::window::internal