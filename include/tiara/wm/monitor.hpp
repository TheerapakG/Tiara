#ifndef TIARA_WM_MONITOR
#define TIARA_WM_MONITOR

#include "tiara/core/stdincludes.hpp"

#include "tiara/core/event/dispatcher.hpp"
#include "tiara/core/vectors.hpp"
#include "tiara/wm/common.hpp"

#include <optional>
#include <set>

namespace tiara::wm {
class Monitor;
}

namespace tiara::wm::events {
    struct MonitorConnectedEvent: core::event::Event {
        const Monitor& monitor;
        using RetType = bool;
    };
    struct MonitorDisconnectedEvent: core::event::Event {
        const Monitor& monitor;
        using RetType = bool;
    };
}

namespace tiara::wm {
class Monitor
{
    public:
    operator GLFWmonitor*() const {
        return _monitor;
    }

    private:
    Monitor(GLFWmonitor* monitor_raw): _monitor{monitor_raw} {
        int w, h;
        glfwGetMonitorWorkarea(_monitor, NULL, NULL, &w, &h);
        detail::logger->info("found monitor: {} ({}x{}) at {}", glfwGetMonitorName(_monitor), w, h, static_cast<void*>(_monitor));
        _register_glfw_callbacks();
    }

    ~Monitor() {
        detail::logger->info("lost monitor: {}", static_cast<void*>(_monitor));
        _unregister_glfw_callbacks();
    }

    void _register_glfw_callbacks() {
        GLFWmonitor* _monitor_raw = _monitor;
        glfwSetMonitorUserPointer(_monitor_raw, this);
    }

    void _unregister_glfw_callbacks() {
        GLFWmonitor* _monitor_raw = _monitor;
        glfwSetMonitorUserPointer(_monitor_raw, NULL);
    }

    friend class MonitorEventDispatcher;

    GLFWmonitor* _monitor;
};

class MonitorEventDispatcher
{
    public:
    static void init() {
        glfwSetMonitorCallback(
            [](GLFWmonitor* _monitor_raw_cb, int event){
                if (!_init) return;
                Monitor* _this = static_cast<Monitor*>(glfwGetMonitorUserPointer(_monitor_raw_cb));
                if (_this == NULL) {
                    _this = new Monitor{_monitor_raw_cb};
                }
                switch (event) {
                case GLFW_CONNECTED:
                    for (auto h: _monitor_connected_handlers) {
                        h.get().handle(events::MonitorConnectedEvent{{}, *_this}, core::event::sync_tag);
                    }
                    break;
                case GLFW_DISCONNECTED:
                    for (auto h: _monitor_disconnected_handlers) {
                        h.get().handle(events::MonitorDisconnectedEvent{{}, *_this}, core::event::sync_tag);
                    }
                    delete _this;
                    break;
                default:
                    break;
                }
            }
        );
        int count;
        GLFWmonitor** monitors = glfwGetMonitors(&count);
        for (int i=0; i<count; i++) {
            Monitor* _this = static_cast<Monitor*>(glfwGetMonitorUserPointer(monitors[i]));
            if (_this == NULL) new Monitor{monitors[i]};
        }
        _init = true;
    }

    static void deinit() {
        _init = false;
        int count;
        GLFWmonitor** monitors = glfwGetMonitors(&count);
        for (int i=0; i<count; i++) {
            Monitor* _this = static_cast<Monitor*>(glfwGetMonitorUserPointer(monitors[i]));
            if (_this != NULL) delete _this;
        }
    }

    static void start_dispatch(core::event::Handler<events::MonitorConnectedEvent>& h) {
        _monitor_connected_handlers.emplace_back(h);
    }
    static void stop_dispatch(core::event::Handler<events::MonitorConnectedEvent>& h) {
        #if TIARA_DETAILS_USE_STD_RANGES_20
            const auto [first, last] = std::ranges::remove_if(_monitor_connected_handlers, [&h](const auto& ref_wrap) { return ref_wrap.get() == h; });
            _monitor_connected_handlers.erase(first, last);
        #else
            const auto last = std::remove_if(_monitor_connected_handlers.begin(), _monitor_connected_handlers.end(), [&h](const auto& ref_wrap) { return ref_wrap.get() == h; });
            _monitor_connected_handlers.erase(last, _monitor_connected_handlers.end());
        #endif
    }
    static void start_dispatch(core::event::Handler<events::MonitorDisconnectedEvent>& h) {
        _monitor_disconnected_handlers.emplace_back(h);
    }
    static void stop_dispatch(core::event::Handler<events::MonitorDisconnectedEvent>& h) {
        #if TIARA_DETAILS_USE_STD_RANGES_20
            const auto [first, last] = std::ranges::remove_if(_monitor_disconnected_handlers, [&h](const auto& ref_wrap) { return ref_wrap.get() == h; });
            _monitor_disconnected_handlers.erase(first, last);
        #else
            const auto last = std::remove_if(_monitor_disconnected_handlers.begin(), _monitor_disconnected_handlers.end(), [&h](const auto& ref_wrap) { return ref_wrap.get() == h; });
            _monitor_disconnected_handlers.erase(last, _monitor_disconnected_handlers.end());
        #endif
    }

    private:
    static inline bool _init = false;
    static inline std::vector<std::reference_wrapper<core::event::Handler<events::MonitorConnectedEvent>>> _monitor_connected_handlers;
    static inline std::vector<std::reference_wrapper<core::event::Handler<events::MonitorDisconnectedEvent>>> _monitor_disconnected_handlers;
};
}

#endif
