#pragma once

#include "engine/RTTI.h"
#include <engine/Event.h>

namespace hpl {

    enum class BrodcastEvent {
        OnPostBufferSwap,
        OnStart,
        OnDraw,
        OnPostRender,
        PreUpdate,
        Update,
        PostUpdate,
        OnQuit,
        OnExit,
        Reset,
        OnPauseUpdate,

        AppGotInputFocus,
        AppGotMouseFocus,
        AppGotVisibility,

        AppLostInputFocus,
        AppLostMouseFocus,
        AppLostVisibility,

        AppDeviceWasPlugged,
        AppDeviceWasRemoved,

        LastEnum
    };

    class IUpdateEventLoop {
        HPL_RTTI_CLASS(IUpdateEventLoop, "{cb713dfb-d6d9-4f41-9021-80af7827f2f1}")
    public:
        using UpdateEvent = hpl::Event<>;

        virtual void Broadcast(BrodcastEvent event) = 0;
        virtual void Subscribe(BrodcastEvent event, UpdateEvent::Handler& handler) = 0;
    };

} // namespace hpl