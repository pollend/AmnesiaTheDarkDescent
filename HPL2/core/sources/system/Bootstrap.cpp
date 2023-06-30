#include <system/Bootstrap.h>

#include <cstdint>
#include <memory>

#include "engine/IUpdateEventLoop.h"
#include "engine/Interface.h"
#include "gui/GuiSet.h"
#include "input/InputKeyboardDevice.h"
#include "input/InputManager.h"

#include <graphics/RendererDeferred.h>

#include "scene/Viewport.h"

#include <input/InputMouseDevice.h>
#include <windowing/NativeWindow.h>

#include "bgfx/platform.h"

namespace hpl {
    Bootstrap::Bootstrap() {
    }
    Bootstrap::~Bootstrap() {
    }

    int32_t Bootstrap::BootstrapThreadHandler(bx::Thread* self, void* _userData) {
        auto bootstrap = reinterpret_cast<Bootstrap*>(_userData);
        int32_t result = bootstrap->m_handler(self);
        self->shutdown();
        return result;
    }

    void Bootstrap::Run(std::function<int32_t(bx::Thread*)> handler) {
        m_handler = handler;
        m_thread.init(BootstrapThreadHandler, this);
        while(m_thread.isRunning()) {
            m_window.Process();
        }

    }

    void Bootstrap::Initialize(BootstrapConfiguration configuration) {

        if (!initMemAlloc("Amnesia"))
        {
            return;
        }
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");

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
        m_window.SetWindowSize(cVector2l(1920, 1080));

        // core renderer initialization
        m_renderer.InitializeRenderer(&m_window);

        // initialize gui rendering
        initResourceLoaderInterface(m_renderer.Rend()); // initializes resources
        m_renderer.InitializeResource();
        gui::InitializeGui(m_renderer);

        // this is safe because the render target is scheduled on the api thread
        m_primaryViewport = std::make_unique<hpl::PrimaryViewport>(m_window);

        // register input devices
        m_inputManager.Register(input::InputManager::KeyboardDeviceID, std::make_shared<input::InputKeyboardDevice>(std::move(keyboardHandle)));
        m_inputManager.Register(input::InputManager::MouseDeviceID, std::make_shared<input::InputMouseDevice>(std::move(mouseHandle)));

        Interface<hpl::ForgeRenderer>::Register(&m_renderer);
        Interface<hpl::PrimaryViewport>::Register(m_primaryViewport.get());
        Interface<input::InputManager>::Register(&m_inputManager);
        Interface<FileReader>::Register(&m_fileReader);
        Interface<FileWriter>::Register(&m_fileWriter);
        Interface<window::NativeWindowWrapper>::Register(&m_window); // storing as a singleton means we can only have one window ...
    }

    void Bootstrap::Shutdown() {
        Interface<hpl::ForgeRenderer>::UnRegister(&m_renderer);
        Interface<hpl::PrimaryViewport>::UnRegister(m_primaryViewport.get());
        Interface<input::InputManager>::UnRegister(&m_inputManager);
        Interface<FileReader>::UnRegister(&m_fileReader);
        Interface<FileWriter>::UnRegister(&m_fileWriter);
        Interface<window::NativeWindowWrapper>::UnRegister(&m_window);
        Interface<IUpdateEventLoop>::UnRegister(&m_updateEventLoop);
    }

}; // namespace hpl
