
#include "engine/Interface.h"
// #include "platform/sdl2/SDL2Mouse.h"
#include <system/Bootstrap.h>

#include <input/InputMouseDevice.h>
#include <windowing/NativeWindow.h>


#include <platform/sdl2/SDL2InputMouseDevice.h>
#include <platform/sdl2/SDL2NativeWindow.h>


namespace hpl {
    Bootstrap::Bootstrap() {
    }
    Bootstrap::~Bootstrap() {
    }

    void Bootstrap::Initialize() {

        // auto* sdl2Window = new hpl::platform::sdl2::SDL2NativeWindow();
        // auto* sdl2Mouse = new hpl::platform::sdl2::SDL2InputMouseDevice();

        // // set internal event handlers
        // m_window.SetWindowInternalEventHandler(sdl2Mouse->GetWindowEventHandle());

        // m_window = std::move(NativeWindow(sdl2Window));
        
        Interface<FileReader>::Register(&m_fileReader);
        Interface<FileWriter>::Register(&m_fileWriter);
        Interface<NativeWindowWrapper>::Register(&m_window);
    }

    void Bootstrap::Shutdown() {

        Interface<FileReader>::UnRegister(&m_fileReader);
        Interface<FileWriter>::UnRegister(&m_fileWriter);
        Interface<NativeWindowWrapper>::UnRegister(&m_window);
    }

    void Bootstrap::Run() {
    }
}; // namespace hpl