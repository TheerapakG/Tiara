#ifndef TIARA_CORE_EVENT_DISPATCHER
#define TIARA_CORE_EVENT_DISPATCHER

#include "tiara/core/event/handler.hpp"
#include "tiara/core/utilities/predicate_combinators.hpp"
#include "tiara/core/utilities/remove_erase.hpp"

#include <numeric>

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

    template <std::derived_from<Event> Ev, typename InitType = size_t>
    struct DefaultDispatcher: public Dispatcher<Ev> {
        public:
        DefaultDispatcher() = default;

        template <typename U>
        DefaultDispatcher(U&& init_value): _init{init_value} {}

        void start_dispatch(Handler<Ev>& h) override {
            _handlers.emplace_back(h);
        }
        void stop_dispatch(Handler<Ev>& h) override {
            utils::remove_erase_if(_handlers, [&h](const auto& ref_wrap) { return ref_wrap.get() == h; });
        }

        protected:
        InitType dispatch(const Ev& event) {
            _dispatch_event_to_results_vec(event);
            return std::accumulate(std::ranges::begin(_results), std::ranges::end(_results), _init);
        }

        template <std::invocable<const InitType&, const typename Ev::RetType&> Op>
        InitType dispatch(const Ev& event, Op op) {
            _dispatch_event_to_results_vec(event);
            return std::accumulate(std::ranges::begin(_results), std::ranges::end(_results), _init, op);
        }

        const std::vector<std::reference_wrapper<Handler<Ev>>>& handlers() const {
            return _handlers;
        }

        private:
        std::vector<std::reference_wrapper<Handler<Ev>>> _handlers;
        std::vector<typename Ev::RetType> _results;
        InitType _init;

        void _dispatch_event_to_results_vec(const Ev& event) {
            _results.clear();
            _results.reserve(_handlers.size());
            auto [begin, end] = std::ranges::transform(
                _handlers,
                std::back_inserter(_results),
                [&event](Handler<Ev>& h) {
                    return h.handle(event, core::event::sync_tag);
                }
            );
            _results.shrink_to_fit();
        }
    };

    template <std::derived_from<Event> Ev>
    struct DelegatingDispatcher: public Dispatcher<Ev> {
        public:
        DelegatingDispatcher(Dispatcher<Ev>& dispatcher): _dispatcher{dispatcher} {}

        void start_dispatch(Handler<Ev>& h) override {
            _dispatcher.start_dispatch(h);
        }
        void stop_dispatch(Handler<Ev>& h) override {
            _dispatcher.stop_dispatch(h);
        }

        private:
        Dispatcher<Ev>& _dispatcher;
    };

    template <std::derived_from<Event> Ev>
    struct DelegatingSharedDispatcher: public Dispatcher<Ev> {
        public:
        DelegatingSharedDispatcher() = default;
        DelegatingSharedDispatcher(std::shared_ptr<Dispatcher<Ev>> dispatcher): _dispatcher{dispatcher} {}

        void start_dispatch(Handler<Ev>& h) override {
            _dispatcher->start_dispatch(h);
        }
        void stop_dispatch(Handler<Ev>& h) override {
            _dispatcher->stop_dispatch(h);
        }

        protected:
        void set_dispatcher(std::shared_ptr<Dispatcher<Ev>> dispatcher) {
            _dispatcher = dispatcher;
        }

        private:
        std::shared_ptr<Dispatcher<Ev>> _dispatcher;
    };

    template <std::derived_from<Event> Ev, typename Executor, HandlerType<Ev, Executor> Hdlr, DispatcherType<Hdlr, Ev, Executor> Dispatch>
    struct KeepAliveDispatcher: virtual public Dispatch {
        template <typename... Args>
        KeepAliveDispatcher(Args&&... args): Dispatch{args...} {}

        void start_dispatch(std::shared_ptr<Hdlr> h) override {
            _keep_shared_alive.emplace_back(std::move(h));
            Dispatch::start_dispatch(*h);
        }
        void stop_dispatch(std::shared_ptr<Hdlr> h) override {
            Dispatch::stop_dispatch(*h);
            utils::remove_erase(_keep_shared_alive, h);
        }

        private:
        std::vector<std::shared_ptr<Hdlr>> _keep_shared_alive;
    } ;
}

#endif
