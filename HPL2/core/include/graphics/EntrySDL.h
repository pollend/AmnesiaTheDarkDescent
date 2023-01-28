#pragma once

#include "bx/thread.h"
#include "math/MathTypes.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class SDL_Window;
union SDL_Event;

namespace hpl::entry_sdl {
    struct Configuration {
        int m_width = 1280;
        int m_height = 720;

        std::string m_name;
    };

    // void setSetupHandler(std::function<void()> handler);
    void setThreadHandler(std::function<int32_t()> handler);

    cVector2l getSize();

    SDL_Window* getWindow();

    void fetchEvents(std::function<void(SDL_Event&)> handler);

    uint32_t getWindowFlags();

    int32_t run(const Configuration& configuration);

    void stop();
}
