#ifndef TIARA_CORE_EVENT_DISPATCHER
#define TIARA_CORE_EVENT_DISPATCHER

#include "tiara/core/event/handler.hpp"
#include "tiara/core/stdincludes.hpp"

namespace tiara::core::event {
    template <typename T, typename Hdlr, typename Ev, typename Executor>
    concept DispatcherType = requires (T t, Hdlr& handler_ref) {
        requires HandlerType<Hdlr, Ev, Executor>;
        { t.start_dispatch(handler_ref) };
        { t.stop_dispatch(handler_ref) };
    };

    template <std::derived_from<Event> Ev, typename Executor = boost::asio::any_io_executor>
    struct AsyncDispatcher {
        virtual ~AsyncDispatcher() = default;
        virtual void start_dispatch(AsyncHandler<Ev, Executor>& h) = 0;
        virtual void stop_dispatch(AsyncHandler<Ev, Executor>& h) = 0;
    };

    template <std::derived_from<Event> Ev>
    struct Dispatcher {
        virtual ~Dispatcher() = default;
        virtual void start_dispatch(Handler<Ev>& h) = 0;
        virtual void stop_dispatch(Handler<Ev>& h) = 0;
    };

    template <std::derived_from<Event> Ev, typename Executor, HandlerType<Ev, Executor> Hdlr, DispatcherType<Hdlr, Ev, Executor> Dispatch>
    struct KeepAliveDispatcher: virtual public Dispatch {
        template <typename... Args>
        KeepAliveDispatcher(Args&&... args): Dispatch{args...} {}

        void start_dispatch(std::shared_ptr<Hdlr> h) {
            _keep_shared_alive.emplace_back(std::move(h));
            start_dispatch(*h);
        }
        void stop_dispatch(std::shared_ptr<Hdlr> h) {
            stop_dispatch(*h);
        #if TIARA_DETAILS_USE_STD_RANGES_20
            const auto [first, last] = std::ranges::remove(_keep_shared_alive, h);
            _keep_shared_alive.erase(first, last);
        #else
            const auto last = std::remove_if(_keep_shared_alive.begin(), _keep_shared_alive.end(), h);
            _keep_shared_alive.erase(last, _keep_shared_alive.end());
        #endif
        }

        private:
        std::vector<std::shared_ptr<Hdlr>> _keep_shared_alive;
    } ;
}

#endif
