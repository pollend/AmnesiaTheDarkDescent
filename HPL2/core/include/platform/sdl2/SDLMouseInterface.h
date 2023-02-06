#pragma once

#include <engine/devices/MouseInterface.h>

namespace hpl {
    class SDLMouse final: public MouseInterface {
    public:
        MouseInterface();
        ~MouseInterface();

        virtual bool ButtonIsDown(MouseButton mButton) override;
        
        virtual cVector2l GetAbsPosition() override;
        virtual cVector2l GetRelPosition() override;

        virtual WindowInterface::WindowInternalEvent::Handler& WindowInternalHandler() override;
    }
}