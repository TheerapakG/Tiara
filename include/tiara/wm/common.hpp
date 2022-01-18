#ifndef TIARA_WM_COMMON
#define TIARA_WM_COMMON

#include "tiara/core/core.hpp"
#include "tiara/core/utilities/predicate_combinators.hpp"

#include "skia/gpu/GrDirectContext.h"
#include "skia/gpu/vk/GrVkBackendContext.h"
#include "skia/gpu/vk/GrVkExtensions.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace tiara::wm::detail {
    static inline auto logger = spdlog::stdout_color_st("tiara::wm");
}

namespace tiara::wm::exceptions {
    struct DeviceQueueSelectionError: public std::runtime_error {
        DeviceQueueSelectionError(): std::runtime_error("error selecting device and queue") {
            detail::logger->error("error selecting device and queue");
        }
    };
}

namespace tiara::wm {
    static inline std::vector<std::string> vulkan_device_extensions{"VK_KHR_swapchain"};
    static inline vk::PhysicalDeviceFeatures vulkan_device_features{};
    static inline std::shared_ptr<vk::raii::PhysicalDevice> preferred_physical_device;
    static inline std::optional<core::Queue> present_queue;
    static inline std::optional<GrVkExtensions> skia_vulkan_extensions;
    static inline std::optional<GrVkBackendContext> skia_vulkan_context;
    static inline sk_sp<GrDirectContext> skia_context;
}

namespace tiara::wm::detail {
    PFN_vkVoidFunction _skia_get_vk_proc(const char* proc_name, VkInstance instance, VkDevice device) {
        if (device != VK_NULL_HANDLE) return vkGetDeviceProcAddr(device, proc_name);
        return vkGetInstanceProcAddr(instance, proc_name); 
    }

    void _setup_skia() {
        auto& ctx = core::context();
        detail::logger->debug("initializing skia");
        skia_vulkan_extensions
            .emplace()
            .init(
                detail::_skia_get_vk_proc,
                *(ctx.vk_instance),
                *(present_queue->device().physical()),
                ctx.vk_extensions.size(),
                ctx.vk_extensions.data(),
                present_queue->device().extensions().size(),
                present_queue->device().extensions().data()
            );
        skia_context = GrDirectContext::MakeVulkan(
            skia_vulkan_context.emplace(
                GrVkBackendContext {
                    .fInstance = *(ctx.vk_instance),
                    .fPhysicalDevice = *(present_queue->device().physical()),
                    .fDevice = **(present_queue->device()),
                    .fQueue = ***present_queue,
                    .fGraphicsQueueIndex = present_queue->family_index(),
                    .fMaxAPIVersion = ctx.vk_api_version(),
                    .fVkExtensions = &(skia_vulkan_extensions.value()),
                    .fDeviceFeatures = reinterpret_cast<VkPhysicalDeviceFeatures*>(&vulkan_device_features),
                    .fGetProc = detail::_skia_get_vk_proc
                }
            )
        );
        detail::logger->debug("initialized skia");
    }
}

namespace tiara::wm {
    std::optional<core::Queue> select_queue_for_surface(std::shared_ptr<vk::raii::PhysicalDevice> physical_device, vk::SurfaceKHR surface) {
        auto& physical_device_ref = *physical_device;
        std::vector<float> queue_priority = {1.0};
        auto queue_families = core::find_queue_families(
            physical_device_ref, 
            core::utils::preds::combinators<uint32_t, const vk::QueueFamilyProperties&>::make_and_(
                core::simple_queue_filter(vk::QueueFlagBits::eGraphics), 
                [&physical_device_ref, &surface](uint32_t queue_index, const vk::QueueFamilyProperties&) -> bool {
                    return physical_device_ref.getSurfaceSupportKHR(queue_index, surface);
                }
            ),
            detail::logger
        );
        if (queue_families.empty()) return std::nullopt;
        auto device_queue_pair = core::create_queues_from_device(
            physical_device,
            vulkan_device_extensions,
            vulkan_device_features,
            {
                {
                    .queueFamilyIndex = queue_families[0],
                    .queueCount       = static_cast<uint32_t>(queue_priority.size()),
                    .pQueuePriorities = queue_priority.data()
                }
            }
        );
        return {std::move(device_queue_pair.second[0][0])};
    }

    void select_device_queue_for_surface(vk::SurfaceKHR surface) {
        if (!present_queue) {
            if (preferred_physical_device) present_queue = select_queue_for_surface(preferred_physical_device, surface);
            else {
                for (
                    auto&& physical_device: 
                    core::find_devices(
                        core::utils::preds::combinators<const core::DevicePropertiesPair&>::make_and_(
                            [](const core::DevicePropertiesPair& physical_device_properties) {
                                auto extension_properties_s = physical_device_properties.first.enumerateDeviceExtensionProperties();
                                std::vector<std::string> extension_names;
                                extension_names.reserve(extension_properties_s.size());

                                std::ranges::transform(
                                    extension_properties_s,
                                    std::back_inserter(extension_names),
                                    [](const vk::ExtensionProperties& extension_properties) -> std::string {
                                        return extension_properties.extensionName;
                                    }
                                );

                                std::ranges::sort(extension_names);

                                std::vector<std::string> required_extension_names{vulkan_device_extensions};
                                std::ranges::sort(required_extension_names);

                                return std::ranges::includes(extension_names, required_extension_names);
                            },
                            [surface](const core::DevicePropertiesPair& physical_device_properties) {
                                return !physical_device_properties.first.getSurfaceFormatsKHR(surface).empty();
                            },
                            [surface](const core::DevicePropertiesPair& physical_device_properties) {
                                return !physical_device_properties.first.getSurfacePresentModesKHR(surface).empty();
                            }
                        ),
                        core::simple_device_comparer,
                        detail::logger
                    )
                ) {
                    present_queue = select_queue_for_surface(std::make_shared<vk::raii::PhysicalDevice>(std::move(physical_device)), surface);
                    if (present_queue) break;
                }
            }

            if (present_queue) {
                detail::_setup_skia();

                if (detail::logger->level() <= spdlog::level::debug) {
                    detail::logger->debug("selected physical device:");
                    auto physical_device_property = present_queue->device().physical().getProperties();
                    detail::logger->debug(
                        "{} (vendor: {}, device: {}, {})",
                        physical_device_property.deviceName,
                        physical_device_property.deviceID,
                        physical_device_property.vendorID,
                        vk::to_string(physical_device_property.deviceType)
                    );
                    detail::logger->debug("selected queue family index {}", present_queue->family_index());
                }
            }
        } else {
            auto& physical_device = present_queue->device().physical();
            if (
                physical_device.getSurfaceFormatsKHR(surface).empty() || 
                physical_device.getSurfacePresentModesKHR(surface).empty()
            ) throw exceptions::DeviceQueueSelectionError{};
        }
        if (!present_queue) throw exceptions::DeviceQueueSelectionError{};
    }
}

#endif
