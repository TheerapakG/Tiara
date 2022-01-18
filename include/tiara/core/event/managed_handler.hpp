#ifndef TIARA_CORE_EVENT_MANAGED_HANDLER
#define TIARA_CORE_EVENT_MANAGED_HANDLER

#include "tiara/core/stdincludes.hpp"
#include "tiara/core/event/dispatcher.hpp"

namespace tiara::core::event {
template <typename Ev, typename Executor, typename ParentHandler /* CRTP */>
struct ManagedHandlerBase;

template <typename Ev, typename Executor, typename ParentHandler /* CRTP */> requires std::derived_from<Ev, Event> && HandlerType<Ev, Executor, ParentHandler>
struct ManagedHandlerBase<Ev, Executor, ParentHandler>
{
    std::vector<std::weak_ptr<AsyncDispatcher<Ev, Executor>>> _weak_subscribed_dispatchers;
};

template <typename Ev, typename Executor, typename ParentHandler /* CRTP */> requires std::derived_from<Ev, Event> && HandlerType<Ev, Executor, ParentHandler> && std::derived_from<ParentHandler, Handler<Ev>>
struct ManagedHandlerBase<Ev, Executor, ParentHandler>
{
    std::vector<std::weak_ptr<AsyncDispatcher<typename ParentHandler::EventType, typename ParentHandler::ExecutorType>>> _weak_subscribed_dispatchers;
    std::vector<std::weak_ptr<Dispatcher<typename ParentHandler::EventType>>> _weak_subscribed_sync_dispatchers;
};

template <std::derived_from<Event> Ev, typename Executor, HandlerType<Ev, Executor> ParentHandler>
class ManagedHandler: virtual public ParentHandler, private ManagedHandlerBase<Ev, Executor, ParentHandler> {
    private:
    using Base = ManagedHandlerBase<Ev, Executor, ParentHandler>;
    public:
    template <typename... Ts>
    ManagedHandler(Ts&&... args): ParentHandler{std::forward<Ts>(args)...} {}

    virtual ~ManagedHandler() {
        for (auto _weak_dispatcher: Base::_weak_subscribed_dispatchers) {
            if (auto _dispatcher = _weak_dispatcher.lock()) _dispatcher->stop_dispatch(static_cast<ParentHandler&>(*this));
        }
        if constexpr(std::derived_from<ParentHandler, Handler<Ev>>) {
            for(auto _weak_sync_dispatcher: Base::_weak_subscribed_sync_dispatchers) {
                if (auto _sync_dispatcher = _weak_sync_dispatcher.lock()) _sync_dispatcher->stop_dispatch(static_cast<ParentHandler&>(*this));
            }
        }
    }
    
    void subscribe(const std::shared_ptr<AsyncDispatcher<Ev, Executor>>& dispatcher) {
        dispatcher->start_dispatch(static_cast<ParentHandler&>(*this));
        Base::_weak_subscribed_dispatchers.emplace_back(dispatcher);
    }

    void unsubscribe(const std::shared_ptr<AsyncDispatcher<Ev, Executor>>& dispatcher) {
        dispatcher->stop_dispatch(static_cast<ParentHandler&>(*this));
        #if TIARA_DETAILS_USE_STD_RANGES_20
            const auto [first, last] = std::ranges::remove_if(
                Base::_weak_subscribed_dispatchers, 
                [&dispatcher](const auto& _weak_dispatcher) {
                    auto _dispatcher = _weak_dispatcher.lock();
                    return (!_dispatcher) || (_dispatcher == dispatcher);
                }
            );
            Base::_weak_subscribed_dispatchers.erase(first, last);
        #else
            const auto last = std::remove_if(
                Base::_weak_subscribed_dispatchers.begin(), 
                Base::_weak_subscribed_dispatchers.end(), 
                [&dispatcher](const auto& _weak_dispatcher) {
                    auto _dispatcher = _weak_dispatcher.lock();
                    return (!_dispatcher) || (_dispatcher == dispatcher);
                }
            );
            Base::_weak_subscribed_dispatchers.erase(
                last,
                Base::_weak_subscribed_dispatchers.end()
            );
        #endif
    }

    template <std::enable_if_t<std::derived_from<ParentHandler, Handler<Ev>>, bool> = true>
    void subscribe(const std::shared_ptr<Dispatcher<Ev>>& dispatcher) {
        dispatcher->start_dispatch(static_cast<ParentHandler&>(*this));
        Base::_weak_subscribed_sync_dispatchers.emplace_back(dispatcher);
    }

    template <std::enable_if_t<std::derived_from<ParentHandler, Handler<Ev>>, bool> = true>
    void unsubscribe(const std::shared_ptr<Dispatcher<Ev>>& dispatcher) {
        dispatcher->stop_dispatch(static_cast<ParentHandler&>(*this));
        #if TIARA_DETAILS_USE_STD_RANGES_20
            const auto [first, last] = std::ranges::remove_if(
                Base::_weak_subscribed_sync_dispatchers, 
                [&dispatcher](const auto& _weak_dispatcher) {
                    auto _dispatcher = _weak_dispatcher.lock();
                    return (!_dispatcher) || (_dispatcher == dispatcher);
                }
            );
            Base::_weak_subscribed_sync_dispatchers.erase(first, last);
        #else
            const auto last = std::remove_if(
                Base::_weak_subscribed_sync_dispatchers.begin(), 
                Base::_weak_subscribed_sync_dispatchers.end(), 
                [&dispatcher](const auto& _weak_dispatcher) {
                    auto _dispatcher = _weak_dispatcher.lock();
                    return (!_dispatcher) || (_dispatcher == dispatcher);
                }
            );
            Base::_weak_subscribed_sync_dispatchers.erase(
                last,
                Base::_weak_subscribed_sync_dispatchers.end()
            );
        #endif
    }
};
}

#endif
