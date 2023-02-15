
#pragma once

#include "bx/thread.h"
#include "engine/UpdateEventLoop.h"
#include "system/Filesystem.h"
#include "windowing/NativeWindow.h"
#include <bx/file.h>
#include <cstdint>
#include <engine/RTTI.h>
#include <functional>
#include <system/Filesystem.h>
#include <input/InputManager.h>

namespace hpl {

    // Bootstrap the engine and provide access to the core systems
    class Bootstrap {
    public:

        Bootstrap();
        ~Bootstrap();

        void Initialize();
        void Run(std::function<int32_t(bx::Thread*)> handler);
        void Shutdown();
    private:
        static int32_t BootstrapThreadHandler(bx::Thread* self, void* _userData);

        UpdateEventLoop m_updateEventLoop;
        input::InputManager m_inputManager;
        window::NativeWindowWrapper m_window;
        FileReader m_fileReader;
        FileWriter m_fileWriter;
        bx::Thread m_thread;
        std::function<int32_t(bx::Thread*)> m_handler;
        bool m_initialized;
    };
}