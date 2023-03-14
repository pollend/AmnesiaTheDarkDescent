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

#pragma once

#include "bx/thread.h"
#include "engine/UpdateEventLoop.h"
#include "system/Filesystem.h"
#include "windowing/NativeWindow.h"
#include <bx/file.h>
#include <cstdint>
#include <engine/RTTI.h>
#include <functional>
#include <memory>
#include <system/Filesystem.h>
#include <input/InputManager.h>

namespace hpl {
    class PrimaryViewport;
    // Bootstrap the engine and provide access to the core systems
    class Bootstrap {
    public:
        struct BootstrapConfiguration final {
        public:
            BootstrapConfiguration() {}

            window::WindowStyle m_windowStyle = window::WindowStyle::WindowStyleNone;
        };

        Bootstrap();
        ~Bootstrap();

        void Initialize(BootstrapConfiguration configuration = BootstrapConfiguration{ } );
        void Run(std::function<int32_t(bx::Thread*)> handler);
        void Shutdown();
    private:
        static int32_t BootstrapThreadHandler(bx::Thread* self, void* _userData);

        std::unique_ptr<PrimaryViewport> m_primaryViewport;
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