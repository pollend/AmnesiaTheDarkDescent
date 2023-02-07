#include "windowing/NativeWindow.h"
#include <bootstrap/WindowingBootstrap.h>
#include <memory>
#include <windowing/NativeWindow.h>
namespace hpl::bootstrap {

    WindowingOutput Initialize(const WindowingOptions& options) {
        WindowingOutput output;
        auto* window = new NativeWindow();
        output.window = window;
        return output;
    }
}