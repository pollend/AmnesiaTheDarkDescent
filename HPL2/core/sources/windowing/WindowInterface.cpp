#include <engine/windowing/WindowInterface.h>
#include <engine/sdl/SDLWindow.h>

namespace hpl::detail {
    static WindowInterface* Create() {
        return new SDLWindow();
    }
}