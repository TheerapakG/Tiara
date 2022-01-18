#ifndef TIARA_CORE_EVENT_HANDLER
#define TIARA_CORE_EVENT_HANDLER

#include "tiara/core/event/eventtype.hpp"

#include <concepts>
#include <functional>
#include <boost/asio.hpp>

#include <memory>

namespace tiara::core::event {
    struct sync_tag_t {};
    static constexpr sync_tag_t sync_tag;

    template <std::derived_from<Event> Ev, typename Executor = boost::asio::any_io_executor>
    struct AsyncHandler {
        AsyncHandler() = default;
        AsyncHandler(AsyncHandler&&) = delete;
        AsyncHandler& operator=(AsyncHandler&&) = delete;

        virtual ~AsyncHandler() = default;

        virtual boost::asio::awaitable<typename Ev::RetType, Executor> handle(const Ev& event) = 0;

        constexpr decltype(auto) operator<=>(const AsyncHandler& rhs) {
            return std::addressof(*this) <=> std::addressof(rhs);
        }

        constexpr bool operator==(const AsyncHandler& rhs) const {
            return std::addressof(*this) == std::addressof(rhs);
        }
    };

    template <std::derived_from<Event> Ev>
    struct Handler: public AsyncHandler<Ev> {
        virtual typename Ev::RetType handle(const Ev& event, sync_tag_t) = 0;

        boost::asio::awaitable<typename Ev::RetType, boost::asio::any_io_executor> handle(const Ev& event) final {
            return ([this, &event]() -> boost::asio::awaitable<typename Ev::RetType, boost::asio::any_io_executor> { co_return handle(event, sync_tag); })();
        }
    };

    template <std::derived_from<Event> Ev, typename Executor, std::convertible_to<std::function<boost::asio::awaitable<typename Ev::RetType, Executor>(const Ev& event)>> F>
    class AsyncFunctionHandler: public AsyncHandler<Ev, Executor> {
        AsyncFunctionHandler(F&& f): f(std::move(f)) {}

        boost::asio::awaitable<typename Ev::RetType, Executor> handle(const Ev& event) final {
            f(event);
        }

        private:
        F f;
    };

    template <std::derived_from<Event> Ev, std::convertible_to<std::function<typename Ev::RetType(const Ev& event)>> F>
    class FunctionHandler: public Handler<Ev> {
        public:
        FunctionHandler(F&& f): f(std::forward<F>(f)) {}

        typename Ev::RetType handle(const Ev& event, sync_tag_t) final {
            return f(event);
        }

        private:
        F f;
    };

    template <std::derived_from<Event> Ev, std::convertible_to<std::function<typename Ev::RetType(const Ev& event)>> F>
    FunctionHandler<Ev, F> make_function_handler(F&& f) {
        return { std::forward<F>(f) };
    }

    template <typename T, typename Ev, typename Executor>
    concept HandlerType = std::derived_from<Ev, Event> && std::derived_from<T, AsyncHandler<Ev, Executor>>;
}

#endif
