#ifndef TIARA_WM_WM
#define TIARA_WM_WM

#include "tiara/wm/monitor.hpp"
#include "tiara/wm/window.hpp"

namespace tiara::wm {
    struct WMExtension: public core::extension::Extension<WMExtension> {
        virtual void init() final {
            detail::logger->info("initializing tiara::wm");
            MonitorEventDispatcher::init();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            _init = true;
            detail::logger->info("initialized tiara::wm");
        }
        virtual void deinit() final {
            detail::logger->info("deinitializing tiara::wm");
            if (skia_context) {
                skia_context->releaseResourcesAndAbandonContext();
                skia_context.reset();
            }
            skia_vulkan_context.reset();
            skia_vulkan_extensions.reset();
            for (auto& [_, semaphore_vec]: detail::_undeleted_semaphores) {
                semaphore_vec.clear();
            }
            present_queue.reset();
            preferred_physical_device = nullptr;
            MonitorEventDispatcher::deinit();
            _init = false;
            detail::logger->info("deinitialized tiara::wm");
        }
        static bool is_init() {
            return _init;
        }
        private:
        static inline bool _init = false;
    };
}

#endif
