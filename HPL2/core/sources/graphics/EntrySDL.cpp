
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_video.h>

#include <bgfx/platform.h>
#include <math/MathTypes.h>
#include <cstdint>
#include <graphics/EntrySDL.h>
#include <mutex>
#include <queue>
#include <string>


namespace hpl::entry_sdl {
    class Context {
    public:
        SDL_Window *m_window = nullptr;

        std::function<int32_t()> m_threadHandler = []() {return 0;};
        std::string m_name;
        bool m_exit = false;
        bx::Thread m_thread;

        // this can be replaced without a mutex
        std::queue<SDL_Event> m_events;
        std::mutex m_event_mutex;

        uint32_t m_windowFlags = 0;

        cVector2l m_size;
    };
    Context g_context;

    	///
	static void* sdlNativeWindowHandle(SDL_Window* window)
	{
		SDL_SysWMinfo wmi;
		SDL_VERSION(&wmi.version);
		if (!SDL_GetWindowWMInfo(window, &wmi) )
		{
			return NULL;
		}

#	if BX_PLATFORM_LINUX || BX_PLATFORM_BSD
#		if ENTRY_CONFIG_USE_WAYLAND
		wl_egl_window *win_impl = (wl_egl_window*)SDL_GetWindowData(_window, "wl_egl_window");
		if(!win_impl)
		{
			int width, height;
			SDL_GetWindowSize(_window, &width, &height);
			struct wl_surface* surface = wmi.info.wl.surface;
			if(!surface)
				return nullptr;
			win_impl = wl_egl_window_create(surface, width, height);
			SDL_SetWindowData(_window, "wl_egl_window", win_impl);
		}
		return (void*)(uintptr_t)win_impl;
#		else
		return (void*)wmi.info.x11.window;
#		endif
#	elif BX_PLATFORM_OSX || BX_PLATFORM_IOS
		return wmi.info.cocoa.window;
#	elif BX_PLATFORM_WINDOWS
		return wmi.info.win.window;
#   elif BX_PLATFORM_ANDROID
		return wmi.info.android.window;
#	endif // BX_PLATFORM_
	}

    void* getNativeDisplayHandle(SDL_Window* window) {
			SDL_SysWMinfo wmi;
		SDL_VERSION(&wmi.version);
		if (!SDL_GetWindowWMInfo(window, &wmi) )
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
		};


    cVector2l getSize() {
        return g_context.m_size;
    }

    SDL_Window* getWindow() {
        return g_context.m_window;
    }

    void setThreadHandler(std::function<int32_t()> handler) {
        g_context.m_threadHandler = handler;
    }

    void fetchEvents(std::function<void(SDL_Event&)> handler) {
        g_context.m_event_mutex.lock();
        while(!g_context.m_events.empty()) {
            handler(g_context.m_events.front());
            g_context.m_events.pop();
        }
        g_context.m_event_mutex.unlock();
    }


    int32_t threadHandler(bx::Thread* self, void* _userData) {
        
        bgfx::Init init;
        #if defined(WIN32)
            // DirectX11 is even more broken then opengl something to consider later ...
		    init.type = bgfx::RendererType::OpenGL;
		#else
		    init.type = bgfx::RendererType::OpenGL;
        #endif
        // init.vendorId = args.m_pciId;
		init.platformData.nwh  = sdlNativeWindowHandle(g_context.m_window);
		init.platformData.ndt  = getNativeDisplayHandle(g_context.m_window);
		init.resolution.width  = g_context.m_size.x;
		init.resolution.height = g_context.m_size.y;
		init.resolution.reset  = BGFX_RESET_VSYNC;
		bgfx::init(init);

        return g_context.m_threadHandler();
    }

    uint32_t getWindowFlags() {
        return g_context.m_windowFlags;
    }

    int32_t run(const Configuration& configuration) {
       
        g_context.m_window = SDL_CreateWindow(configuration.m_name.c_str(),
                                    SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,
                                    configuration.m_width, configuration.m_height, 
                                    SDL_WINDOW_SHOWN | 
                                    SDL_WINDOW_RESIZABLE);
        int w,h;
        SDL_GetWindowSize(g_context.m_window, &w, &h);
        g_context.m_size = cVector2l(w, h);
        g_context.m_thread.init(threadHandler, nullptr);
        bgfx::renderFrame();

        SDL_Event event;
        while(!g_context.m_exit) {
	        bgfx::renderFrame(1000);
            g_context.m_windowFlags = SDL_GetWindowFlags(hpl::entry_sdl::getWindow());

            g_context.m_event_mutex.lock();
            while (SDL_PollEvent(&event) )
            {
                g_context.m_events.push(event);
                switch(event.type)
                {
                    case SDL_QUIT:
                        g_context.m_exit = true;
                        break;
                    
                    default:
                        break;
                }
            }
            g_context.m_event_mutex.unlock();
        }

        while (bgfx::RenderFrame::NoContext != bgfx::renderFrame() ) {};
        g_context.m_thread.shutdown();

        SDL_DestroyWindow(g_context.m_window);
        SDL_Quit();

        return g_context.m_thread.getExitCode();
    }

    void stop() {
        g_context.m_thread.shutdown();

    }
}
