#ifndef TIARA_WM_WINDOW
#define TIARA_WM_WINDOW

#include "tiara/core/stdincludes.hpp"

#include "tiara/common/events/draw.hpp"
#include "tiara/core/core.hpp"
#include "tiara/core/event/dispatcher.hpp"
#include "tiara/core/vectors.hpp"
#include "tiara/wm/common.hpp"

#include "skia/core/SkSurface.h"
#include "skia/gpu/GrBackendSemaphore.h"

#include <algorithm>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>

namespace tiara::wm::events {
    struct WindowPosEvent: core::iVec2D, core::event::Event {
        using RetType = bool;
    };
    struct WindowSizeEvent: core::iVec2D, core::event::Event {
        using RetType = bool;
    };
    struct WindowCloseEvent: core::event::Event {
        using RetType = bool;
    };
    struct WindowRefreshEvent: core::event::Event {
        using RetType = bool;
    };
    struct WindowFocusEvent: core::event::Event {
        using RetType = bool;
        bool focus;
    };
    struct WindowMinimizeEvent: core::event::Event {
        using RetType = bool;
        bool minimize;
    };
    struct WindowMaximizeEvent: core::event::Event {
        using RetType = bool;
        bool maximize;
    };
    struct WindowFramebufferSizeEvent: core::iVec2D, core::event::Event {
        using RetType = bool;
    };
    struct WindowScaleEvent: core::fVec2D, core::event::Event {
        using RetType = bool;
    };
}

namespace tiara::wm::exceptions {
    struct CreateWindowError: public std::runtime_error {
        CreateWindowError(const char* description): std::runtime_error(description) {}
    };

    struct DrawWindowError: public std::runtime_error {
        DrawWindowError(const char* description): std::runtime_error(description) {}
    };

    struct CreateWindowGLFWError: public CreateWindowError {
        public:
        static CreateWindowGLFWError get_error() {
            const char* desc_ptr;
            int error_code = glfwGetError(&desc_ptr);
            return CreateWindowGLFWError{error_code, desc_ptr};
        }

        int error_code;

        private:
        CreateWindowGLFWError(int error_code, const char* description):
            error_code(error_code),
            CreateWindowError(description)
        {}
    };
}

namespace tiara::wm {
    struct Monitor;
}

namespace tiara::wm::detail {
    static inline std::map<void*, std::vector<std::pair<vk::raii::Semaphore, GrBackendSemaphore>>> _undeleted_semaphores;
class Window: 
    public core::event::DefaultDispatcher<
        events::WindowPosEvent, 
        events::WindowSizeEvent,
        events::WindowCloseEvent,
        events::WindowRefreshEvent,
        events::WindowFocusEvent,
        events::WindowMinimizeEvent,
        events::WindowMaximizeEvent,
        events::WindowFramebufferSizeEvent,
        events::WindowScaleEvent,
        common::events::DrawEvent
    >
{
    public:
    using DefaultDispatcherT = core::event::DefaultDispatcher<
        events::WindowPosEvent, 
        events::WindowSizeEvent,
        events::WindowCloseEvent,
        events::WindowRefreshEvent,
        events::WindowFocusEvent,
        events::WindowMinimizeEvent,
        events::WindowMaximizeEvent,
        events::WindowFramebufferSizeEvent,
        events::WindowScaleEvent,
        common::events::DrawEvent
    >;

    Window(
        core::iVec2D size,
        const std::string& title,
        const std::optional<std::reference_wrapper<tiara::wm::Monitor>>& monitor
    );

    Window(const Window&) = delete;
    Window(Window&&) = delete;

    virtual ~Window() {
        detail::logger->info("destroying window: {}", static_cast<void*>(_window_raw));
        if (_run || current_frames_enqueued > 0) {
            auto [it, success] = _undeleted_semaphores.emplace(static_cast<void*>(this), std::vector<std::pair<vk::raii::Semaphore, GrBackendSemaphore>>{});
            if (!success) {
                detail::logger->warn("window {}: window might not get stopped properly", static_cast<void*>(_window_raw));
                detail::logger->debug("window {}: (run: {}, frames_enqueued: {})", static_cast<void*>(_window_raw), _run, current_frames_enqueued);
            } else {
                it->second.reserve(
                    _window_swapchain_image_renderable_semaphores.size() +
                    _window_swapchain_image_rendered_semaphores.size() +
                    _window_swapchain_image_presentable_semaphores.size()
                );
                it->second.insert(
                    it->second.end(),
                    std::make_move_iterator(_window_swapchain_image_renderable_semaphores.begin()),
                    std::make_move_iterator(_window_swapchain_image_renderable_semaphores.end())
                );
                it->second.insert(
                    it->second.end(),
                    std::make_move_iterator(_window_swapchain_image_rendered_semaphores.begin()),
                    std::make_move_iterator(_window_swapchain_image_rendered_semaphores.end())
                );
                it->second.insert(
                    it->second.end(),
                    std::make_move_iterator(_window_swapchain_image_presentable_semaphores.begin()),
                    std::make_move_iterator(_window_swapchain_image_presentable_semaphores.end())
                );
            }
        }
        _unregister_glfw_callbacks();
        glfwDestroyWindow(_window_raw);
        detail::logger->info("destroyed window: {}", static_cast<void*>(_window_raw));
    }

    Window& operator=(const Window&) = delete;
    Window& operator=(Window&&) = delete;

    operator GLFWwindow*() {
        return _window_raw;
    }

    using DefaultDispatcherT::start_dispatch;
    using DefaultDispatcherT::stop_dispatch;

    void start_dispatch(core::event::Handler<common::events::DrawEvent>& h) final {
        _window_draw_handler.emplace(h);
    }
    void stop_dispatch(core::event::Handler<common::events::DrawEvent>& h) final {
        if (_window_draw_handler && _window_draw_handler.value().get() == h) _window_draw_handler.reset();
    }

    void draw() {
        if (current_frames_enqueued >= max_frames_enqueued) return;
        if (!_window_draw_handler || !_run) return;
        if (current_image == std::numeric_limits<uint32_t>::max()) {
            auto [result, next_image] = _window_swapchain.acquireNextImage(0, *(_window_swapchain_image_renderable_semaphores.back().first));
            if (result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR) {
                current_image = next_image;
                std::swap(_window_swapchain_image_renderable_semaphores[current_image], _window_swapchain_image_renderable_semaphores.back());
            }
            else return;
        }
        while (!_window_skia_surfaces[current_image]->wait(1, &_window_swapchain_image_renderable_semaphores[current_image].second, false));
        _window_draw_handler->get().handle(common::events::DrawEvent{{}, _window_skia_surfaces[current_image]->getCanvas()}, core::event::sync_tag);
        if (
            skia_context->flush(
                GrFlushInfo {
                    .fNumSemaphores = 1,
                    .fSignalSemaphores = &_window_swapchain_image_rendered_semaphores[current_image].second
                }
            ) == GrSemaphoresSubmitted::kNo
        ) {
            detail::logger->error("window {}: skia cannot flush semaphores to submit", static_cast<void*>(_window_raw));
            // throw exceptions::DrawWindowError{"skia cannot flush semaphores to submit"};
        }
        if (!skia_context->submit()) {
            detail::logger->error("window {}: skia cannot submit semaphores to queue", static_cast<void*>(_window_raw));
            // throw exceptions::DrawWindowError{"skia cannot submit semaphores to queue"};
        }
        while (!_window_skia_surfaces[current_image]->wait(1, &_window_swapchain_image_rendered_semaphores[current_image].second, false));
        if (
            !skia_context->setBackendRenderTargetState(
                _window_skia_backend_render_targets[current_image],
                {static_cast<VkImageLayout>(vk::ImageLayout::ePresentSrcKHR), VK_QUEUE_FAMILY_IGNORED}
            )
        ) {
            detail::logger->error("window {}: skia cannot transition image to present source", static_cast<void*>(_window_raw));
        }
        if (
            skia_context->flush(
                GrFlushInfo {
                    .fNumSemaphores = 1,
                    .fSignalSemaphores = &_window_swapchain_image_presentable_semaphores[current_image].second,
                    .fFinishedProc = [](GrGpuFinishedContext finish_context){
                        _undeleted_semaphores.erase(finish_context);
                    },
                    .fFinishedContext = this
                }
            ) == GrSemaphoresSubmitted::kNo
        ) {
            current_frames_enqueued++;
            detail::logger->error("window {}: skia cannot flush semaphores to submit", static_cast<void*>(_window_raw));
            // throw exceptions::DrawWindowError{"skia cannot flush semaphores to submit"};
        }
        if (!skia_context->submit()) {
            detail::logger->error("window {}: skia cannot submit semaphores to queue", static_cast<void*>(_window_raw));
            // throw exceptions::DrawWindowError{"skia cannot submit semaphores to queue"};
        }
        current_frames_enqueued++;
        auto result = present_queue.value()->presentKHR({
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &(*_window_swapchain_image_presentable_semaphores[current_image].first),
            .swapchainCount = 1,
            .pSwapchains = &(*_window_swapchain),
            .pImageIndices = &current_image
        });
        current_image = std::numeric_limits<uint32_t>::max();
        if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
            _recreate_swapchain();
        } else if (result != vk::Result::eSuccess) {
            detail::logger->error("window {}: cannot present image ({})", static_cast<void*>(_window_raw), result);
            throw exceptions::DrawWindowError{"cannot present image"};
        }
    }
    void stop() {
        _run = false;
    }
    private:
    std::optional<std::reference_wrapper<core::event::Handler<common::events::DrawEvent>>> _window_draw_handler;

    static void _glfw_window_pos_callback(GLFWwindow* _window_raw_cb, int xpos, int ypos) {
        Window* _this = static_cast<Window*>(glfwGetWindowUserPointer(_window_raw_cb));
        if (_this == NULL) return;
        _this->DefaultDispatcherT::dispatch(events::WindowPosEvent{{xpos, ypos}}, 0);
    }
    static void _glfw_window_size_callback(GLFWwindow* _window_raw_cb, int width, int height) { 
        Window* _this = static_cast<Window*>(glfwGetWindowUserPointer(_window_raw_cb));
        if (_this == NULL) return;
        _this->DefaultDispatcherT::dispatch(events::WindowSizeEvent{{width, height}}, 0);
    }
    static void _glfw_window_close_callback(GLFWwindow* _window_raw_cb) {
        Window* _this = static_cast<Window*>(glfwGetWindowUserPointer(_window_raw_cb));
        if (_this == NULL) return;
        _this->DefaultDispatcherT::dispatch(events::WindowCloseEvent{}, 0);
    }
    static void _glfw_window_refresh_callback(GLFWwindow* _window_raw_cb) {
        Window* _this = static_cast<Window*>(glfwGetWindowUserPointer(_window_raw_cb));
        if (_this == NULL) return;
        _this->DefaultDispatcherT::dispatch(events::WindowRefreshEvent{}, 0);
    }
    static void _glfw_window_focus_callback(GLFWwindow* _window_raw_cb, int focused) {
        Window* _this = static_cast<Window*>(glfwGetWindowUserPointer(_window_raw_cb));
        if (_this == NULL) return;
        _this->DefaultDispatcherT::dispatch(events::WindowFocusEvent{}, 0);
    }
    static void _glfw_window_iconify_callback(GLFWwindow* _window_raw_cb, int iconified) {
        Window* _this = static_cast<Window*>(glfwGetWindowUserPointer(_window_raw_cb));
        if (_this == NULL) return;
        _this->DefaultDispatcherT::dispatch(events::WindowMinimizeEvent{}, 0);
    }
    static void _glfw_window_maximize_callback(GLFWwindow* _window_raw_cb, int maximized) {
        Window* _this = static_cast<Window*>(glfwGetWindowUserPointer(_window_raw_cb));
        if (_this == NULL) return;
        _this->DefaultDispatcherT::dispatch(events::WindowMaximizeEvent{}, 0);
    }
    static void _glfw_window_framebuffer_size_callback(GLFWwindow* _window_raw_cb, int width, int height) {
        Window* _this = static_cast<Window*>(glfwGetWindowUserPointer(_window_raw_cb));
        if (_this == NULL) return;
        _this->DefaultDispatcherT::dispatch(events::WindowFramebufferSizeEvent{width, height}, 0);
    }
    static void _glfw_window_content_scale_callback(GLFWwindow* _window_raw_cb, float xscale, float yscale) {
        Window* _this = static_cast<Window*>(glfwGetWindowUserPointer(_window_raw_cb));
        if (_this == NULL) return;
        _this->DefaultDispatcherT::dispatch(events::WindowScaleEvent{xscale, yscale}, 0);
    }

    void _register_glfw_callbacks() {
        glfwSetWindowUserPointer(_window_raw, this); // cannot use lambda capturing `this` in callbacks because glfw expect c-style ptr, so store it in user pointer
        glfwSetWindowPosCallback(_window_raw, _glfw_window_pos_callback);
        glfwSetWindowSizeCallback(_window_raw, _glfw_window_size_callback);
        glfwSetWindowCloseCallback(_window_raw, _glfw_window_close_callback);
        glfwSetWindowRefreshCallback(_window_raw, _glfw_window_refresh_callback);
        glfwSetWindowFocusCallback(_window_raw, _glfw_window_focus_callback);
        glfwSetWindowIconifyCallback(_window_raw, _glfw_window_iconify_callback);
        glfwSetWindowMaximizeCallback(_window_raw, _glfw_window_maximize_callback);
        glfwSetFramebufferSizeCallback(_window_raw, _glfw_window_framebuffer_size_callback);
        glfwSetWindowContentScaleCallback(_window_raw, _glfw_window_content_scale_callback);
    }
    void _unregister_glfw_callbacks() {
        glfwSetWindowUserPointer(_window_raw, NULL);
        glfwSetWindowPosCallback(_window_raw, NULL);
        glfwSetWindowSizeCallback(_window_raw, NULL);
        glfwSetWindowCloseCallback(_window_raw, NULL);
        glfwSetWindowRefreshCallback(_window_raw, NULL);
        glfwSetWindowFocusCallback(_window_raw, NULL);
        glfwSetWindowIconifyCallback(_window_raw, NULL);
        glfwSetWindowMaximizeCallback(_window_raw, NULL);
        glfwSetFramebufferSizeCallback(_window_raw, NULL);
        glfwSetWindowContentScaleCallback(_window_raw, NULL);
    }

    void _recreate_swapchain() {
        auto& device = present_queue->device();
        device->waitIdle();
        auto surface_capabilities = device.physical().getSurfaceCapabilitiesKHR(*_window_surface);
        uint32_t swapchain_image_count = std::min(
            surface_capabilities.minImageCount + 1,
            surface_capabilities.maxImageCount == 0 ? std::numeric_limits<uint32_t>::max() : surface_capabilities.maxImageCount
        );
        detail::logger->debug(
            "window {}: selecting swapchain minimum image count {} (min: {}, max {})",
            static_cast<void*>(_window_raw),
            swapchain_image_count,
            surface_capabilities.minImageCount,
            surface_capabilities.maxImageCount
        );

        auto available_surface_formats = device.physical().getSurfaceFormatsKHR(*_window_surface);
        if (detail::logger->level() <= spdlog::level::debug) {
            detail::logger->debug("available image formats:");
            for (auto& format: available_surface_formats) {
                detail::logger->debug(
                    "{} {}",
                    vk::to_string(format.format),
                    vk::to_string(format.colorSpace)
                );
            }
        }
        auto swapchain_image_format_it = std::ranges::find_if(
            available_surface_formats, 
            [](const vk::SurfaceFormatKHR& format){
                return format.format == vk::Format::eB8G8R8A8Unorm && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
            }
        );
        if (swapchain_image_format_it == available_surface_formats.end()) {
            exceptions::CreateWindowError error{"cannot find suitable image format for swapchain"};
            detail::logger->error("error intitializing window {}: {}", static_cast<void*>(_window_raw), error.what());
            throw error;
        }
        detail::logger->debug(
            "window {}: selecting swapchain image format {} {}",
            static_cast<void*>(_window_raw),
            vk::to_string(swapchain_image_format_it->format),
            vk::to_string(swapchain_image_format_it->colorSpace)
        );

        int width, height;
        glfwGetFramebufferSize(_window_raw, &width, &height);

        vk::Extent2D swapchain_image_extent {
            .width = std::clamp(static_cast<uint32_t>(width), surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width),
            .height = std::clamp(static_cast<uint32_t>(height), surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height)
        };
        _window_swapchain_extent = {static_cast<int>(swapchain_image_extent.width), static_cast<int>(swapchain_image_extent.height)};
        detail::logger->debug(
            "window {}: selecting swapchain image extent {}x{} (min: {}x{}, max: {}x{})",
            static_cast<void*>(_window_raw),
            swapchain_image_extent.width,
            swapchain_image_extent.height,
            surface_capabilities.minImageExtent.width,
            surface_capabilities.minImageExtent.height,
            surface_capabilities.maxImageExtent.width,
            surface_capabilities.maxImageExtent.height
        );
        
        auto available_surface_present_modes = device.physical().getSurfacePresentModesKHR(*_window_surface);
        auto surface_present_mode_it = std::ranges::find(
            available_surface_present_modes, 
            vk::PresentModeKHR::eMailbox
        );
        auto swapchain_image_present_mode = (surface_present_mode_it == available_surface_present_modes.end() ? vk::PresentModeKHR::eFifo : *surface_present_mode_it);
        detail::logger->debug(
            "window {}: selecting swapchain image present mode {}",
            static_cast<void*>(_window_raw),
            vk::to_string(swapchain_image_present_mode)
        );

        _window_swapchain = {
            *device,
            {
                .surface = *_window_surface,
                .minImageCount = swapchain_image_count,
                .imageFormat = swapchain_image_format_it->format,
                .imageColorSpace = swapchain_image_format_it->colorSpace,
                .imageExtent = swapchain_image_extent,
                .imageArrayLayers = 1,
                .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
                .imageSharingMode = vk::SharingMode::eExclusive,
                .preTransform = surface_capabilities.currentTransform,
                .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
                .presentMode = swapchain_image_present_mode,
                .clipped = true,
                .oldSwapchain = *_window_swapchain
            }
        };
        detail::logger->debug("window {}: created swapchain", static_cast<void*>(_window_raw));

        current_image = std::numeric_limits<uint32_t>::max();

        detail::logger->debug("window {}: getting images", static_cast<void*>(_window_raw));
        _window_swapchain_images = _window_swapchain.getImages();
        detail::logger->debug("window {}: got images", static_cast<void*>(_window_raw));

        _window_skia_surfaces.clear();
        _window_skia_backend_render_targets.clear();

        detail::logger->debug("window {}: creating skia backend render targets", static_cast<void*>(_window_raw));
        auto _window_skia_backend_render_targets_view = std::ranges::subrange{_window_swapchain_images.begin(), _window_swapchain_images.end()} |
            std::views::transform(
                [this, &swapchain_image_format_it](const VkImage& image){
                    return GrBackendRenderTarget {
                        _window_swapchain_extent.x,
                        _window_swapchain_extent.y,
                        GrVkImageInfo {
                            .fImage = image,
                            .fAlloc = GrVkAlloc{},
                            .fImageTiling = static_cast<VkImageTiling>(vk::ImageTiling::eOptimal),
                            .fImageLayout = static_cast<VkImageLayout>(vk::ImageLayout::eUndefined),
                            .fFormat = static_cast<VkFormat>(swapchain_image_format_it->format),
                            .fImageUsageFlags = static_cast<VkImageUsageFlags>(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst),
                            .fLevelCount = 1,
                            .fCurrentQueueFamily = present_queue->family_index(),
                            .fSharingMode = static_cast<VkSharingMode>(vk::SharingMode::eExclusive),
                            #ifdef SK_BUILD_FOR_ANDROID_FRAMEWORK
                            .fPartOfSwapchainOrAndroidWindow = true
                            #endif
                        }
                    };
                }
            );
        _window_skia_backend_render_targets.reserve(_window_skia_backend_render_targets_view.size());
        _window_skia_backend_render_targets.insert(_window_skia_backend_render_targets.end(), _window_skia_backend_render_targets_view.begin(), _window_skia_backend_render_targets_view.end());
        detail::logger->debug("window {}: created skia backend render targets", static_cast<void*>(_window_raw));

        detail::logger->debug("window {}: creating skia surfaces", static_cast<void*>(_window_raw));
        auto _window_skia_surfaces_view = std::ranges::subrange{_window_skia_backend_render_targets.begin(), _window_skia_backend_render_targets.end()} |
            std::views::transform(
                [this](const GrBackendRenderTarget& backend_render_target){
                    auto skia_surface = SkSurface::MakeFromBackendRenderTarget(
                        skia_context.get(),
                        backend_render_target,
                        GrSurfaceOrigin::kTopLeft_GrSurfaceOrigin,
                        SkColorType::kBGRA_8888_SkColorType,
                        SkColorSpace::MakeSRGB(),
                        nullptr
                    );
                    if (!skia_surface) {
                        exceptions::CreateWindowError error{"cannot create skia surface from swapchain image"};
                        detail::logger->error("error intitializing window {}: {}", static_cast<void*>(_window_raw), error.what());
                        throw error;
                    }
                    return skia_surface;
                }
            );
        _window_skia_surfaces.reserve(_window_skia_surfaces_view.size());
        _window_skia_surfaces.insert(_window_skia_surfaces.end(), _window_skia_surfaces_view.begin(), _window_skia_surfaces_view.end());
        detail::logger->debug("window {}: created skia surfaces", static_cast<void*>(_window_raw));

        size_t swapchain_images_size = _window_swapchain_images.size();
        size_t swapchain_image_renderable_semaphores_size = _window_swapchain_image_renderable_semaphores.size();
        if (swapchain_image_renderable_semaphores_size < swapchain_images_size + 1) {
            detail::logger->debug(
                "window {}: creating image renderable semaphores ({} -> {})",
                static_cast<void*>(_window_raw),
                swapchain_image_renderable_semaphores_size,
                swapchain_images_size + 1
            );
            _window_swapchain_image_renderable_semaphores.reserve(swapchain_images_size + 1);
            std::ranges::transform(
                std::views::iota(swapchain_image_renderable_semaphores_size, swapchain_images_size + 1),
                std::back_inserter(_window_swapchain_image_renderable_semaphores),
                [this, &device](uint32_t) -> std::pair<vk::raii::Semaphore, GrBackendSemaphore> {
                    auto vk_semaphore = device->createSemaphore({});
                    GrBackendSemaphore skia_semaphore;
                    skia_semaphore.initVulkan(*vk_semaphore);
                    return {std::move(vk_semaphore), std::move(skia_semaphore)};
                }
            );
            detail::logger->debug("window {}: created image renderable semaphores ({})", static_cast<void*>(_window_raw), _window_swapchain_image_renderable_semaphores.size());
        }
        size_t swapchain_image_rendered_semaphores_size = _window_swapchain_image_rendered_semaphores.size();
        if (swapchain_image_rendered_semaphores_size < swapchain_images_size) {
            detail::logger->debug(
                "window {}: creating image rendered semaphores ({} -> {})",
                static_cast<void*>(_window_raw),
                swapchain_image_rendered_semaphores_size,
                swapchain_images_size
            );
            _window_swapchain_image_rendered_semaphores.reserve(swapchain_images_size);
            std::ranges::transform(
                std::views::iota(swapchain_image_rendered_semaphores_size, swapchain_images_size),
                std::back_inserter(_window_swapchain_image_rendered_semaphores),
                [this, &device](uint32_t) -> std::pair<vk::raii::Semaphore, GrBackendSemaphore> {
                    auto vk_semaphore = device->createSemaphore({});
                    GrBackendSemaphore skia_semaphore;
                    skia_semaphore.initVulkan(*vk_semaphore);
                    return {std::move(vk_semaphore), std::move(skia_semaphore)};
                }
            );
            detail::logger->debug("window {}: created image rendered semaphores ({})", static_cast<void*>(_window_raw), _window_swapchain_image_rendered_semaphores.size());
        }
        size_t swapchain_image_presentable_semaphores_size = _window_swapchain_image_presentable_semaphores.size();
        if (swapchain_image_presentable_semaphores_size < swapchain_images_size) {
            detail::logger->debug(
                "window {}: creating image presentable semaphores ({} -> {})",
                static_cast<void*>(_window_raw),
                swapchain_image_presentable_semaphores_size,
                swapchain_images_size
            );
            _window_swapchain_image_presentable_semaphores.reserve(swapchain_images_size);
            std::ranges::transform(
                std::views::iota(swapchain_image_presentable_semaphores_size, swapchain_images_size),
                std::back_inserter(_window_swapchain_image_presentable_semaphores),
                [this, &device](uint32_t) -> std::pair<vk::raii::Semaphore, GrBackendSemaphore> {
                    auto vk_semaphore = device->createSemaphore({});
                    GrBackendSemaphore skia_semaphore;
                    skia_semaphore.initVulkan(*vk_semaphore);
                    return {std::move(vk_semaphore), std::move(skia_semaphore)};
                }
            );
            detail::logger->debug("window {}: created image presentable semaphores ({})", static_cast<void*>(_window_raw), _window_swapchain_image_presentable_semaphores.size());
        }
        max_frames_enqueued = swapchain_images_size - 1;
        detail::logger->debug("window {}: max frames {}", static_cast<void*>(_window_raw), max_frames_enqueued);
    }

    GLFWwindow* _window_raw;
    vk::raii::SurfaceKHR _window_surface;
    vk::raii::SwapchainKHR _window_swapchain;
    core::iVec2D _window_swapchain_extent;
    std::vector<VkImage> _window_swapchain_images;
    std::vector<std::pair<vk::raii::Semaphore, GrBackendSemaphore>> _window_swapchain_image_renderable_semaphores;
    std::vector<std::pair<vk::raii::Semaphore, GrBackendSemaphore>> _window_swapchain_image_rendered_semaphores;
    std::vector<std::pair<vk::raii::Semaphore, GrBackendSemaphore>> _window_swapchain_image_presentable_semaphores;
    std::vector<GrBackendRenderTarget> _window_skia_backend_render_targets;
    std::vector<sk_sp<SkSurface>> _window_skia_surfaces;
    bool _run = true;
    size_t current_frames_enqueued = 0;
    size_t max_frames_enqueued = 0;
    uint32_t current_image = std::numeric_limits<uint32_t>::max();
};
}

namespace tiara::wm {
class Window: 
    public core::event::DelegatingSharedDispatcher<
        detail::Window,
        events::WindowPosEvent,
        events::WindowSizeEvent,
        events::WindowCloseEvent,
        events::WindowRefreshEvent,
        events::WindowFocusEvent,
        events::WindowMinimizeEvent,
        events::WindowMaximizeEvent,
        events::WindowFramebufferSizeEvent,
        events::WindowScaleEvent,
        common::events::DrawEvent
    >
{
    public:
    Window(
        core::iVec2D size,
        const std::string& title,
        const std::optional<std::reference_wrapper<tiara::wm::Monitor>>& monitor = std::nullopt
    );

    explicit operator GLFWwindow*() {
        return static_cast<GLFWwindow*>(*_window_detail);
    }

    void draw() {
        _window_detail->draw();
    }
    void stop() {
        _window_detail->stop();
    }
    private:
    std::shared_ptr<detail::Window> _window_detail;
};
}

#include "tiara/wm/monitor.hpp"

namespace tiara::wm::detail {
Window::Window(
    core::iVec2D size,
    const std::string& title,
    const std::optional<std::reference_wrapper<tiara::wm::Monitor>>& monitor
):
    _window_raw {
        (
            [&](){
                detail::logger->info("creating window: {} ({}x{})", title, size.x, size.y);
                auto _window_raw = glfwCreateWindow(size.x, size.y, title.c_str(), monitor? static_cast<GLFWmonitor*>(monitor.value().get()) : NULL, NULL);
                if (_window_raw == NULL) {
                    auto error = exceptions::CreateWindowGLFWError::get_error();
                    detail::logger->error("error creating window: {} ({}x{}) {}", title, size.x, size.y, error.what());
                    throw error;
                }
                detail::logger->info("created window: {} ({}x{}) at {}", title, size.x, size.y, static_cast<void*>(_window_raw));
                return _window_raw;
            }
        )()
    },
    _window_surface {
        core::context().vk_instance,
        (
            [&](){
                VkSurfaceKHR _window_surface;
                glfwCreateWindowSurface(*core::context().vk_instance, _window_raw, NULL, &_window_surface);
                select_device_queue_for_surface(_window_surface);
                return _window_surface;
            }
        )()
    },
    _window_swapchain{nullptr}
{
    _register_glfw_callbacks();
    _recreate_swapchain();
}
}

namespace tiara::wm {
Window::Window(
    core::iVec2D size,
    const std::string& title,
    const std::optional<std::reference_wrapper<tiara::wm::Monitor>>& monitor
):
    _window_detail{std::make_shared<detail::Window>(size, title, monitor)}
{
    DelegatingSharedDispatcher::dispatcher() = _window_detail;
}
}

#endif
