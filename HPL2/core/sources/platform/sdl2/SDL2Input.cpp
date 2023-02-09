#include <input/InputMouseDevice.h>

#include "math/MathTypes.h"
#include "windowing/NativeWindow.h"
#include <input/InputMouseDevice.h>
#include <memory>

#include <SDL2/SDL_events.h>

namespace hpl::input::internal {
   struct NativeInputImpl {
        window::internal::WindowInternalEvent::Handler m_windowEventHandle;
   };

    InputMouseHandle Initialize() {
        InputMouseHandle handle = InputMouseHandle {
            InputMouseHandle::Ptr(new NativeInputImpl(), [](void* ptr) {
                auto impl = static_cast<NativeInputImpl*>(ptr);
                delete impl;
            })
        };

        auto* impl = static_cast<NativeInputImpl*>(handle.Get());

        impl->m_windowEventHandle = window::internal::WindowInternalEvent::Handler([](hpl::window::InternalEvent& event) {
            auto& sdlEvent = *event.m_sdlEvent;

            switch (sdlEvent.type) {
            case SDL_MOUSEMOTION:
                break;

            }
        });

        return handle;
    }
    cVector2l GetAbsPosition(InputMouseHandle& handle) {

    }
    cVector2l GetRelPosition(InputMouseHandle& handle) {

    }


    window::internal::WindowInternalEvent::Handler& GetWindowEventHandle(InputMouseHandle& handle) {
        auto* impl = static_cast<NativeInputImpl*>(handle.Get());
        return impl->m_windowEventHandle;
    }

}