#include <system/Bootstrap.h>

#include <cstdint>
#include <memory>

#include "engine/IUpdateEventLoop.h"
#include "engine/Interface.h"
#include "input/InputKeyboardDevice.h"
#include "input/InputManager.h"

#include "scene/Viewport.h"


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
		auto windowSize = bootstrap->m_window.GetWindowSize();
        init.platformData.nwh  = bootstrap->m_window.NativeWindowHandle();
		init.platformData.ndt  = bootstrap->m_window.NativeDisplayHandle();
		init.resolution.width  = windowSize.x;
		init.resolution.height = windowSize.y;
		init.resolution.reset  = BGFX_RESET_VSYNC;
        bgfx::init(init);
        int32_t result = bootstrap->m_handler(self);
        self->shutdown();
        bootstrap->m_primaryViewport->Invalidate();
        bgfx::shutdown();
        return result;
    }

    void Bootstrap::Run(std::function<int32_t(bx::Thread*)> handler) {
        m_handler = handler;
        m_thread.init(BootstrapThreadHandler, this);
        while(m_thread.isRunning()) {
            bgfx::renderFrame();
            m_window.Process();
        }
    }

    void Bootstrap::Initialize(BootstrapConfiguration configuration) {
        Interface<IUpdateEventLoop>::Register(&m_updateEventLoop);
        
        auto keyboardHandle = hpl::input::internal::keyboard::Initialize();
        auto mouseHandle = hpl::input::internal::mouse::Initialize();
        auto windowHandle = hpl::window::internal::Initialize(configuration.m_windowStyle);

        // set internal event handlers
        hpl::window::internal::ConnectInternalEventHandler(windowHandle,
            hpl::input::internal::keyboard::GetWindowEventHandle(keyboardHandle));
        hpl::window::internal::ConnectInternalEventHandler(windowHandle,
            hpl::input::internal::mouse::GetWindowEventHandle(mouseHandle));

        m_window = hpl::window::NativeWindowWrapper(std::move(windowHandle));

        // this is safe because the render target is scheduled on the api thread
        m_primaryViewport = std::make_unique<hpl::PrimaryViewport>(m_window);
        
        // register input devices
        m_inputManager.Register(input::InputManager::KeyboardDeviceID, std::make_shared<input::InputKeyboardDevice>(std::move(keyboardHandle)));
        m_inputManager.Register(input::InputManager::MouseDeviceID, std::make_shared<input::InputMouseDevice>(std::move(mouseHandle)));

        Interface<hpl::PrimaryViewport>::Register(m_primaryViewport.get());
        Interface<input::InputManager>::Register(&m_inputManager);
        Interface<FileReader>::Register(&m_fileReader);
        Interface<FileWriter>::Register(&m_fileWriter);
        Interface<window::NativeWindowWrapper>::Register(&m_window); // storing as a singleton means we can only have one window ...
    }

    void Bootstrap::Shutdown() {
        
        Interface<hpl::PrimaryViewport>::UnRegister(m_primaryViewport.get());
        Interface<input::InputManager>::UnRegister(&m_inputManager);
        Interface<FileReader>::UnRegister(&m_fileReader);
        Interface<FileWriter>::UnRegister(&m_fileWriter);
        Interface<window::NativeWindowWrapper>::UnRegister(&m_window);
        Interface<IUpdateEventLoop>::UnRegister(&m_updateEventLoop);
    }

}; // namespace hpl