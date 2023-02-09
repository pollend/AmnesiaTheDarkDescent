
#pragma once

#include "system/Filesystem.h"
#include "windowing/NativeWindow.h"
#include <bx/file.h>
#include <engine/RTTI.h>
#include <system/Filesystem.h>
#include <input/InputManager.h>

namespace hpl {

    class Bootstrap {
    public:

        Bootstrap();
        ~Bootstrap();

        void Initialize();
        void Shutdown();
        void Run();
    private:
        hpl::window::NativeWindowWrapper m_window;
        FileReader m_fileReader;
        FileWriter m_fileWriter;
        input::InputManager m_inputManager;
        bool m_initialized;
    };
}