
#pragma once

// Core Event Loop
#include "engine/Event.h"
#include <engine/RTTI.h>
#include <engine/IUpdateEventLoop.h>

namespace hpl {

class UpdateEventLoop final : public IUpdateEventLoop {
    HPL_RTTI_CLASS(InputDevice, "{eb97278b-a219-4051-b3b5-d30f95230d46}")
public:
   
    UpdateEventLoop();
    ~UpdateEventLoop();

    virtual void Broadcast(BrodcastEvent event) override {
        
    }
    virtual void Subscribe(BrodcastEvent event, UpdateEvent::Handler& handler) override {
        
    }

private:
    std::array<UpdateEvent, static_cast<size_t>(BrodcastEvent::LastEnum)> m_events;
};
}