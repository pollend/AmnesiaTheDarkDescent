#pragma once

#include <windowing/WindowInterface.h>
#include <memory>

namespace hpl {

    class SDLWindow final : public WindowInterface {
        struct Impl;
    public:
        SDLWindow();
        ~SDLWindow() override;

        virtual void* NativeWindowHandle() override;
        virtual void* NativeDisplayHandle() override;
    
        virtual void SetWindowInternalEventHandler(WindowInternalEvent::Handler& handler) override;
        virtual void SetWindowEventHandler(WindowEvent::Handler& handler) override;
        
        virtual void Process() override;

        virtual void SetWindowSize(cVector2l size) override;

        virtual WindowType GetWindowType() override;
        virtual cVector2l GetWindowSize() override;
    private:
        WindowInternalEvent m_internalWindowEvent;
        WindowEvent m_windowEvent;
        std::unique_ptr<Impl> m_impl;
    };
} // namespace hpl