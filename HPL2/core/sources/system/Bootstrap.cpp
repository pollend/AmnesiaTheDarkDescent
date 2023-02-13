#include <system/Bootstrap.h>

#include "engine/IUpdateEventLoop.h"
#include "engine/Interface.h"
#include "input/InputKeyboardDevice.h"
#include "input/InputManager.h"
// #include "platform/sdl2/SDL2Mouse.h"
#include <cstdint>
#include <memory>

#include <input/InputMouseDevice.h>
#include <windowing/NativeWindow.h>

#include "bgfx/bgfx.h"
#include "bgfx/platform.h"
namespace hpl {
    Bootstrap::Bootstrap() {
    }
    Bootstrap::~Bootstrap() {
    }

     int32_t Bootstrap::BootstrapThreadHandler(bx::Thread* self, void* _userData) {
        auto bootstrap = reinterpret_cast<Bootstrap*>(_userData);
        
        bgfx::Init init;
        #if defined(WIN32)
            // DirectX11 is even more broken then opengl something to consider later ...
		    init.type = bgfx::RendererType::OpenGL;
		#else
		    init.type = bgfx::RendererType::OpenGL;
        #endif
		init.platformData.nwh  = bootstrap->m_window.NativeWindowHandle();
		init.platformData.ndt  = bootstrap->m_window.NativeDisplayHandle();
		init.resolution.width  = 1280;
		init.resolution.height = 800;
		init.resolution.reset  = BGFX_RESET_VSYNC;
        bgfx::init(init);
        return bootstrap->m_handler(self);
    }

    void Bootstrap::Run(std::function<int32_t(bx::Thread*)> handler) {
        m_handler = handler;
        volatile bool isRunning = true;
        hpl::window::WindowEvent::Handler windowEventHandler = hpl::window::WindowEvent::Handler([&](window::WindowEventPayload& payload) {
            switch(payload.m_type) {
                case window::WindowEventType::QuitEvent:
                    isRunning = false;
                    break;
                default:
                    break;
            }
        });
        m_window.SetWindowEventHandler(windowEventHandler);

        m_thread.init(BootstrapThreadHandler, this);
        while(isRunning) {
            bgfx::renderFrame(1000);
            m_window.Process();
        }
        m_thread.shutdown();
    }

    void Bootstrap::Initialize() {
        Interface<IUpdateEventLoop>::Register(&m_updateEventLoop);
        
        auto keyboardHandle = hpl::input::internal::keyboard::Initialize();
        auto mouseHandle = hpl::input::internal::mouse::Initialize();
        auto windowHandle = hpl::window::internal::Initialize();

        // set internal event handlers
        hpl::window::internal::ConnectInternalEventHandler(windowHandle,
            hpl::input::internal::keyboard::GetWindowEventHandle(keyboardHandle));
        hpl::window::internal::ConnectInternalEventHandler(windowHandle,
            hpl::input::internal::mouse::GetWindowEventHandle(mouseHandle));

        m_window = hpl::window::NativeWindowWrapper(std::move(windowHandle));
        
        // register input devices
        m_inputManager.Register(input::InputManager::KeyboardDeviceID, std::make_shared<input::InputKeyboardDevice>(std::move(keyboardHandle)));
        m_inputManager.Register(input::InputManager::MouseDeviceID, std::make_shared<input::InputMouseDevice>(std::move(mouseHandle)));

        Interface<input::InputManager>::Register(&m_inputManager);
        Interface<FileReader>::Register(&m_fileReader);
        Interface<FileWriter>::Register(&m_fileWriter);
        Interface<window::NativeWindowWrapper>::Register(&m_window);
    }

    void Bootstrap::Shutdown() {
        while (bgfx::RenderFrame::NoContext != bgfx::renderFrame() ) {};
        Interface<input::InputManager>::UnRegister(&m_inputManager);
        Interface<FileReader>::UnRegister(&m_fileReader);
        Interface<FileWriter>::UnRegister(&m_fileWriter);
        Interface<window::NativeWindowWrapper>::UnRegister(&m_window);
        Interface<IUpdateEventLoop>::UnRegister(&m_updateEventLoop);
    }

}; // namespace hpl