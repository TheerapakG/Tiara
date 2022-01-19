#include "spdlog/spdlog.h"

#include "tiara/core/event/managed_handler.hpp"

struct Event: tiara::core::event::Event {
    using RetType = bool;
};

struct EventHandler: tiara::core::event::Handler<Event> {
    EventHandler(int function_num): function_num{function_num} {}

    ~EventHandler() override {
        spdlog::info("{} destroyed!", function_num);
    }

    bool handle(const Event& event, tiara::core::event::sync_tag_t) override {
        spdlog::info("{} received event!", function_num);
        return true;
    }

    int function_num;
};

struct EventDispatcher: tiara::core::event::Dispatcher<Event> {
    virtual ~EventDispatcher() {
        spdlog::info("dispatcher destroyed!");
    }
    void start_dispatch(tiara::core::event::Handler<Event>& h) override {
        _event_handlers.emplace_back(h);
    }
    void stop_dispatch(tiara::core::event::Handler<Event>& h) override {
        if (auto h_casted = dynamic_cast<EventHandler*>(&h)) {
            spdlog::info("unregistering handler {}!", h_casted->function_num);
        } else {
            spdlog::info("unregistering a handler!");
        }
        tiara::core::utils::remove_erase_if(_event_handlers, [&h](const auto& ref_wrap) { return ref_wrap.get() == h; });
    }
    void emit() {
        spdlog::info("emitting!");
        for (auto h: _event_handlers) h.get().handle(Event{}, tiara::core::event::sync_tag);
    }
    std::vector<std::reference_wrapper<tiara::core::event::Handler<Event>>> _event_handlers;
};

int main() {
    spdlog::set_pattern("[%Y-%m-%d %T.%e] [logger %n] [thread %t] [%^%l%$] %v");
    auto handler_1 = std::make_shared<tiara::core::event::ManagedHandler<Event, boost::asio::any_io_executor, EventHandler>>(1);
    auto handler_2 = std::make_shared<tiara::core::event::ManagedHandler<Event, boost::asio::any_io_executor, EventHandler>>(2);
    auto handler_3 = std::make_shared<tiara::core::event::ManagedHandler<Event, boost::asio::any_io_executor, EventHandler>>(3);
    auto dispatcher = std::make_shared<EventDispatcher>();
    handler_1->subscribe(dispatcher);
    handler_2->subscribe(dispatcher);
    dispatcher->emit(); // 1 2
    handler_1.reset();
    dispatcher->emit(); // 2
    handler_3->subscribe(dispatcher);
    dispatcher->emit(); // 2 3
    handler_2->unsubscribe(dispatcher);
    dispatcher->emit(); // 3
}
