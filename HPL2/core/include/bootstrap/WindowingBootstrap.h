#pragma once

#include "windowing/NativeWindow.h"
#include <memory>
#include <string_view>

namespace hpl {
    class NativeWindow;
namespace bootstrap {
    struct WindowingOptions {
        std::string_view title;
    };
    struct WindowingOutput {
        NativeWindow* window;
    };
    static WindowingOutput Initialize(const WindowingOptions& options);
}
}