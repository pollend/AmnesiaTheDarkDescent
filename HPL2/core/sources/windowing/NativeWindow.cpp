#include "math/MathTypes.h"
#include <windowing/NativeWindow.h>

namespace hpl {

    NativeWindow::NativeWindow()
        : m_impl(NativeWindow::CreateWindow()) {
    }

    void* NativeWindow::NativeWindowHandle() {
        return nullptr;
    }
    
    void* NativeWindow::NativeDisplayHandle() {
        return nullptr;
    }

    void NativeWindow::SetWindowSize(cVector2l size) {
    }

    void NativeWindow::SetWindowInternalEventHandler(WindowInternalEvent::Handler& handler) {
        m_impl->SetWindowInternalEventHandler(handler);
    }

    void NativeWindow::SetWindowEventHandler(WindowEvent::Handler& handler) {
        m_impl->SetWindowEventHandler(handler);
    }

    void NativeWindow::Process() {
        m_impl->Process();
    }

    WindowType NativeWindow::GetWindowType() {
        return WindowType::Window;
    }

    cVector2l NativeWindow::GetWindowSize() {
        return cVector2l(0, 0);
    }
} // namespace hpl