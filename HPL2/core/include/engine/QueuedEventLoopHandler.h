
#pragma once

#include "engine/Event.h"
#include "engine/IUpdateEventLoop.h"
#include "engine/Interface.h"
#include "engine/UpdateEventLoop.h"

namespace hpl {

/**
* Events are queued and processed on the event loop.
*/
template<typename... Params>
class QueuedEventLoopHandler
{
public:
    using TargetEvent = hpl::Event<Params...>;

    struct Options {
    public:
        /**
        * called before processing queued events
        */
        std::function<void()> onBegin = [](){};
        /**
        * called after all queued events have been processed
        */
        std::function<void()> onEnd = [](){};
        /**
        * a filter function that returns true if the event should be queued
        */
        std::function<bool(Params...)> filter = [](Params...){ return true; };
    };
    inline QueuedEventLoopHandler(BroadcastEvent event, TargetEvent::Callback callback, const Options options = Options {}):
        m_dispatchHandler([&, options, callback](float value) {
            std::lock_guard<std::mutex> lock(m_mutex);
            options.onBegin();
            while(!m_queuedEvents.empty()) {
                auto& data = m_queuedEvents.front();
                std::apply(callback, data);
                m_queuedEvents.pop();
            }
            options.onEnd();
        }),
        m_handler([&, options](Params... params) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if(options.filter(params...)) {
                m_queuedEvents.emplace(std::tuple<Params...>(params...));
            }
        }), m_broadcastEvent(event)
    {
    }

    void Connect(TargetEvent& event) {
        Interface<IUpdateEventLoop>::Get()->Subscribe(m_broadcastEvent, m_dispatchHandler);
        m_handler.Connect(event);
    }

    QueuedEventLoopHandler(const QueuedEventLoopHandler&) = delete;
    QueuedEventLoopHandler(QueuedEventLoopHandler&&) = delete;
    QueuedEventLoopHandler& operator=(const QueuedEventLoopHandler&) = delete;
    QueuedEventLoopHandler& operator=(QueuedEventLoopHandler&&) = delete;
private:
    hpl::IUpdateEventLoop::UpdateEvent::Handler m_dispatchHandler;
    hpl::Event<Params...>::Handler m_handler;
    std::queue<std::tuple<typename std::remove_reference<Params>::type...>> m_queuedEvents;
    std::mutex m_mutex;
    BroadcastEvent m_broadcastEvent;
};

} // namespace hpl
