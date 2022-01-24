#ifndef TIARA_CORE_CORE
#define TIARA_CORE_CORE

#include "tiara/core/event/event.hpp"
#include "tiara/core/extension/extension.hpp"
#include "tiara/core/utilities/concept_invocable.hpp"

#include "spdlog/sinks/stdout_color_sinks.h"

#include <ranges>
#include <set>

namespace tiara::core::detail {
    static inline uint32_t vulkan_api_version = VK_API_VERSION_1_2;
}

namespace tiara::core {
    template <typename... Exts>
    class Tiara;

    struct Context {
        Context(
            vk::ApplicationInfo app_info,
            std::vector<const char*> instance_layers,
            std::vector<const char*> instance_extensions
        ):
            vk_instance {
                (
                    [&](){
                        TIARA_SUPPRESS_LSAN_SCOPE
                        return vk::raii::Instance {
                            vk_context,
                            {
                                .pApplicationInfo = &app_info,
                                .enabledLayerCount = static_cast<uint32_t>(instance_layers.size()),
                                .ppEnabledLayerNames = instance_layers.data(),
                                .enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size()),
                                .ppEnabledExtensionNames = instance_extensions.data(),
                            }
                        };
                    }
                )()
            },
            vk_extensions {std::move(instance_extensions)},
            vk_layers {std::move(instance_layers)} 
        {}

        uint32_t vk_api_version() const {
            return detail::vulkan_api_version;
        }

        vk::raii::Context vk_context;
        vk::raii::Instance vk_instance;
        const std::vector<const char*> vk_extensions;
        const std::vector<const char*> vk_layers;

        private:
        std::vector<std::optional<extension::detail::ExtensionInitHandle>> tiara_exts;

        template <typename... Exts>
        friend class ::tiara::core::Tiara;
    };
}

namespace tiara::core::detail {
    static inline auto logger = spdlog::stdout_color_st("tiara::core");

    static inline std::optional<Context> ctx;
}

namespace tiara::core::exceptions {
    struct TiaraInitError: public std::runtime_error {
        TiaraInitError(const char* description): std::runtime_error(description) {
            detail::logger->error("error initializing tiara: {}", description);
        }
    };

    struct TiaraGLFWInitError: public TiaraInitError {
        public:
        static TiaraGLFWInitError get_error() {
            const char* desc_ptr;
            int error_code = glfwGetError(&desc_ptr);
            return TiaraGLFWInitError{error_code, desc_ptr};
        }

        int error_code;

        private:
        TiaraGLFWInitError(int error_code, const char* description):
            error_code(error_code),
            TiaraInitError(description)
        {}
    };
}

namespace tiara::core {
    static inline std::string application_name{"Tiara Application"};
    static inline std::tuple<uint32_t, uint32_t, uint32_t> application_version{1, 0, 0};
    static inline std::vector<std::string> vulkan_instance_layers;
    static inline std::vector<std::string> vulkan_instance_extensions;

    template <typename... Exts>
    struct Tiara: public extension::Extension<Tiara<Exts...>> {
        virtual void init() final {
            detail::logger->info("initializing tiara");
            step_or_rollback(
                [](){
                    detail::logger->debug("initializing glfw");
                    if (!glfwInit()) throw exceptions::TiaraGLFWInitError::get_error();
                    detail::logger->debug("initialized glfw");
                },
                [](){ 
                    detail::logger->debug("deinitializing glfw");
                    glfwTerminate();
                    detail::logger->debug("deinitialized glfw");
                }
            );
            step_or_rollback(
                [](){
                    uint32_t extensions_count;
                    auto extensions = glfwGetRequiredInstanceExtensions(&extensions_count);

                    if (extensions == NULL) throw exceptions::TiaraGLFWInitError::get_error();

                    detail::logger->debug("available vulkan instance layers:");
                    for (auto& layer_property: vk::enumerateInstanceLayerProperties()) {
                        detail::logger->debug("{}: {}.{}.{}", layer_property.layerName, VK_VERSION_MAJOR(layer_property.specVersion), VK_VERSION_MINOR(layer_property.specVersion), VK_VERSION_PATCH(layer_property.specVersion));
                    }

                    detail::logger->debug("available vulkan instance extension:");
                    for (auto& extension_property: vk::enumerateInstanceExtensionProperties()) {
                        detail::logger->debug("{}: {}", extension_property.extensionName, extension_property.specVersion);
                    }
                    
                    detail::logger->debug("creating vulkan instance:");

                    std::vector<const char*> transformed_layers;
                    if (!vulkan_instance_layers.empty()) {
                        auto layers_view = std::ranges::subrange{vulkan_instance_layers.begin(), vulkan_instance_layers.end()} |
                            std::views::transform([](const std::string& s){ return s.c_str(); });
                        transformed_layers.reserve(layers_view.size());
                        transformed_layers.insert(transformed_layers.end(), std::ranges::begin(layers_view), std::ranges::end(layers_view));
                    }
                    detail::logger->debug("layers:");
                    for (auto layer: transformed_layers) detail::logger->debug("{}", layer);

                    if (vulkan_instance_extensions.empty()) {
                        detail::logger->debug("extensions:");
                        for (uint32_t i = 0; i < extensions_count; i++) detail::logger->debug("{}", extensions[i]);
                        detail::ctx.emplace(
                            vk::ApplicationInfo {
                                .pApplicationName = application_name.c_str(),
                                .applicationVersion = VK_MAKE_VERSION(std::get<0>(application_version), std::get<1>(application_version), std::get<2>(application_version)),
                                .pEngineName = "Tiara Engine",
                                .engineVersion = VK_MAKE_VERSION(0, 1, 0),
                                .apiVersion = detail::vulkan_api_version,
                            },
                            std::move(transformed_layers),
                            std::vector<const char*> {extensions, extensions+extensions_count}
                        );
                    } else {
                        std::vector<std::string> combine_extensions;
                        combine_extensions.reserve(extensions_count+vulkan_instance_extensions.size());
                        combine_extensions.insert(combine_extensions.end(), extensions, extensions+extensions_count);
                        combine_extensions.insert(combine_extensions.end(), vulkan_instance_extensions.begin(), vulkan_instance_extensions.end());

                        std::ranges::sort(combine_extensions);
                        auto combine_extensions_end = std::ranges::unique(combine_extensions).begin();
                        auto transformed_extensions_view = std::ranges::subrange{combine_extensions.begin(), combine_extensions_end} |
                            std::views::transform([](const std::string& s){ return s.c_str(); });
                        
                        std::vector<const char*> transformed_extensions;
                        transformed_extensions.reserve(transformed_extensions_view.size());
                        transformed_extensions.insert(transformed_extensions.end(), std::ranges::begin(transformed_extensions_view), std::ranges::end(transformed_extensions_view));

                        detail::logger->debug("extensions:");
                        for (auto& extension: transformed_extensions) detail::logger->debug("{}", extension);
                        detail::ctx.emplace(
                            vk::ApplicationInfo {
                                .pApplicationName = application_name.c_str(),
                                .applicationVersion = VK_MAKE_VERSION(std::get<0>(application_version), std::get<1>(application_version), std::get<2>(application_version)),
                                .pEngineName = "Tiara Engine",
                                .engineVersion = VK_MAKE_VERSION(0, 1, 0),
                                .apiVersion = VK_API_VERSION_1_2,
                            },
                            std::move(transformed_layers),
                            std::move(transformed_extensions)
                        );
                    }
                    
                    detail::logger->debug("created vulkan instance:");
                },
                [](){
                    detail::logger->debug("destroying vulkan instance");
                    detail::ctx.reset();
                    detail::logger->debug("destroyed vulkan instance");
                }
            );
            step_or_rollback(
                [](){
                    detail::logger->info("initializing tiara extensions");
                    auto& exts_ref = detail::ctx.value().tiara_exts;
                    exts_ref.reserve(sizeof...(Exts));
                    (exts_ref.emplace_back(Exts::init_ext()),...);
                    detail::logger->info("initialized tiara extensions");
                },
                [](){
                    detail::logger->info("deinitializing tiara extensions");
                    auto& exts_ref = detail::ctx.value().tiara_exts;
                    while (!exts_ref.empty()) exts_ref.pop_back();
                    detail::logger->info("deinitialized tiara extensions");
                }
            );
            detail::logger->info("initialized tiara");
        }
        virtual void deinit() final {
            detail::logger->info("deinitializing tiara");
            rollback();
            detail::logger->info("deinitialized tiara");
        }
        static bool is_init() {
            return static_cast<bool>(detail::ctx);
        }
        private:
        template <utils::InvocableR<void> FInit, utils::InvocableR<void> FDeinit>
        static void step_or_rollback(FInit finit, FDeinit fdeinit) {
            try {
                finit();
                deinit_func.emplace_back(fdeinit);
            }  catch(...) {
                rollback();
                std::rethrow_exception(std::current_exception());
            }
        }

        static void rollback() {
            try {
                while (!deinit_func.empty()) {
                    deinit_func.back()();
                    deinit_func.pop_back();
                }
            }  catch(...) {
                std::rethrow_exception(std::current_exception());
            }
        }

        static inline std::vector<std::function<void(void)>> deinit_func;
    };

    const Context& context() {
        return detail::ctx.value();
    }

    struct Device {
        Device(std::shared_ptr<vk::raii::PhysicalDevice> physical_device, vk::raii::Device&& device, std::vector<const char*> extensions): 
            _physical_device{std::move(physical_device)},
            _device{std::move(device)},
            _extensions{std::move(extensions)}
        {}

        vk::raii::Device& operator*() noexcept {
            return _device;
        }

        vk::raii::Device* operator->() noexcept {
            return &_device;
        }

        const vk::raii::Device& operator*() const noexcept {
            return _device;
        }

        const vk::raii::Device* operator->() const noexcept {
            return &_device;
        }

        vk::raii::PhysicalDevice& physical() const noexcept {
            return *_physical_device;
        }

        const std::vector<const char*>& extensions() const noexcept {
            return _extensions;
        }
        
        private:
        std::shared_ptr<vk::raii::PhysicalDevice> _physical_device;
        vk::raii::Device _device;
        const std::vector<const char*> _extensions;
    };

    struct Queue {
        Queue(std::shared_ptr<Device> device, uint32_t family_index, vk::raii::Queue&& queue): 
            _device{std::move(device)},
            _family_index{family_index},
            _queue{std::move(queue)}
        {}

        vk::raii::Queue& operator*() noexcept {
            return _queue;
        }

        vk::raii::Queue* operator->() noexcept {
            return &_queue;
        }

        const vk::raii::Queue& operator*() const noexcept {
            return _queue;
        }

        const vk::raii::Queue* operator->() const noexcept {
            return &_queue;
        }

        Device& device() const noexcept {
            return *_device;
        }

        uint32_t family_index() const noexcept {
            return _family_index;
        }

        private:
        std::shared_ptr<Device> _device;
        uint32_t _family_index;
        vk::raii::Queue _queue;
    };

    using DevicePropertiesPair = std::pair<vk::raii::PhysicalDevice, vk::PhysicalDeviceProperties>;

    template <
        utils::InvocableR<bool, const DevicePropertiesPair&> DeviceFilterFunc,
        utils::InvocableR<bool, const DevicePropertiesPair&, const DevicePropertiesPair&> DeviceCompareFunc
    >
    std::vector<vk::raii::PhysicalDevice> find_devices (
        DeviceFilterFunc&& device_filter_func,
        DeviceCompareFunc&& device_compare_func,
        std::shared_ptr<spdlog::logger> logger = nullptr
    ) {
        auto physical_devices = vk::raii::PhysicalDevices{detail::ctx.value().vk_instance};

        std::vector<DevicePropertiesPair> physical_device_pairs;
        physical_device_pairs.reserve(physical_devices.size());

        std::ranges::transform(
            std::make_move_iterator(physical_devices.begin()),
            std::make_move_iterator(physical_devices.end()),
            std::back_inserter(physical_device_pairs),
            [](vk::raii::PhysicalDevice&& physical_device) -> DevicePropertiesPair {
                auto properties = physical_device.getProperties();
                return {std::move(physical_device), properties};
            }
        );

        if (logger && logger->level() <= spdlog::level::debug) {
            logger->debug("physical devices:");
            for (auto& physical_device_pair: physical_device_pairs) {
                logger->debug(
                    "{} (vendor: {}, device: {}, {})",
                    physical_device_pair.second.deviceName,
                    physical_device_pair.second.deviceID,
                    physical_device_pair.second.vendorID,
                    vk::to_string(physical_device_pair.second.deviceType)
                );
            }
        }

        auto filtered_range = std::ranges::subrange{
            std::ranges::begin(physical_device_pairs),
            std::ranges::begin(
                std::ranges::remove_if(
                    physical_device_pairs,
                    [&device_filter_func](auto& physical_device_pair){
                        return !device_filter_func(physical_device_pair); 
                    }
                )
            )
        };
        std::ranges::sort(filtered_range, device_compare_func);

        std::vector<vk::raii::PhysicalDevice> result_devices;
        physical_device_pairs.reserve(filtered_range.size());

        std::ranges::move(filtered_range | std::ranges::views::keys, std::back_inserter(result_devices));

        return result_devices;
    }

    template <
        utils::InvocableR<bool, uint32_t, const vk::QueueFamilyProperties&> QueueFamilyFilterFunc
    > 
    std::vector<uint32_t> find_queue_families (
        const vk::raii::PhysicalDevice& physical_device,
        QueueFamilyFilterFunc&& queue_filter_func,
        std::shared_ptr<spdlog::logger> logger = nullptr
    ) {
        auto queue_family_properties = physical_device.getQueueFamilyProperties();

        if (logger && logger->level() <= spdlog::level::debug) {
            logger->debug("queue families for {}:", physical_device.getProperties().deviceName);
            for (uint32_t i = 0; auto& queue_family_property: queue_family_properties) {
                logger->debug(
                    "{}: flags: {}, timestamp bits: {}, minimum image transfer granularity: ({}, {}, {}) x {}",
                    i,
                    vk::to_string(queue_family_property.queueFlags),
                    queue_family_property.timestampValidBits,
                    queue_family_property.minImageTransferGranularity.width,
                    queue_family_property.minImageTransferGranularity.height,
                    queue_family_property.minImageTransferGranularity.depth,
                    queue_family_property.queueCount
                );
                i++;
            }
        }

        auto filtered_family_indices = std::views::iota(uint32_t{0}, static_cast<uint32_t>(queue_family_properties.size())) |
            std::ranges::views::transform(
                [&queue_family_properties](uint32_t i) -> std::pair<uint32_t, std::reference_wrapper<vk::QueueFamilyProperties>> {
                    return {i, queue_family_properties[i]};
                }
            ) |
            std::ranges::views::filter(
                [&queue_filter_func](std::pair<uint32_t, std::reference_wrapper<vk::QueueFamilyProperties>> indexed_queue_family_property) {
                    return queue_filter_func(indexed_queue_family_property.first, indexed_queue_family_property.second.get());
                }
            ) |
            std::ranges::views::keys;

        std::vector<uint32_t> result_queues;
        result_queues.reserve(queue_family_properties.size());
        result_queues.insert(result_queues.end(), std::ranges::begin(filtered_family_indices), std::ranges::end(filtered_family_indices));
        result_queues.shrink_to_fit();

        return result_queues;
    }

    std::pair<
        std::shared_ptr<Device>,
        std::vector<std::vector<Queue>>
    > create_queues_from_device (
        std::shared_ptr<vk::raii::PhysicalDevice> physical_device,
        const std::vector<std::string>& device_extensions,
        const vk::PhysicalDeviceFeatures& device_features,
        const std::vector<vk::DeviceQueueCreateInfo>& queue_create_info
    ) {
        auto& context = detail::ctx.value();
        std::vector<const char*> transformed_extensions;
        if (!device_extensions.empty()) {
            auto extensions_view = std::ranges::subrange{device_extensions.begin(), device_extensions.end()} |
                std::views::transform([](const std::string& s){ return s.c_str(); });
            transformed_extensions.reserve(extensions_view.size());
            transformed_extensions.insert(transformed_extensions.end(), std::ranges::begin(extensions_view), std::ranges::end(extensions_view));
        }
        auto device = std::make_shared<Device>(
            physical_device,
            vk::raii::Device {
                *physical_device,
                {
                    .queueCreateInfoCount = static_cast<uint32_t>(queue_create_info.size()),
                    .pQueueCreateInfos = queue_create_info.data(),
                    .enabledLayerCount = static_cast<uint32_t>(context.vk_layers.size()),
                    .ppEnabledLayerNames = context.vk_layers.data(),
                    .enabledExtensionCount = static_cast<uint32_t>(transformed_extensions.size()),
                    .ppEnabledExtensionNames = transformed_extensions.data(),
                    .pEnabledFeatures = &device_features,
                }
            },
            std::move(transformed_extensions)
        );

        auto queues_view = queue_create_info |
            std::views::transform(
                [&device](const vk::DeviceQueueCreateInfo& queue_info) {
                    uint32_t queue_family_index = queue_info.queueFamilyIndex;

                    auto queues_view = std::views::iota(uint32_t{0}, queue_info.queueCount) |
                        std::views::transform(
                            [&device, queue_family_index](uint32_t queue_index) -> Queue {
                                return {
                                    device,
                                    queue_family_index,
                                    (*device)->getQueue(queue_family_index, queue_index)
                                };
                            }
                        );

                    std::vector<Queue> queues;
                    queues.reserve(queue_info.queueCount);
                    queues.insert(queues.end(), std::ranges::begin(queues_view), std::ranges::end(queues_view));

                    return queues;
                }
            );

        std::vector<std::vector<Queue>> queues;
        queues.reserve(queue_create_info.size());
        queues.insert(queues.end(), std::ranges::begin(queues_view), std::ranges::end(queues_view));

        return {std::move(device), std::move(queues)};
    }

    bool simple_device_comparer(const DevicePropertiesPair& lhs, const DevicePropertiesPair& rhs) {
        constexpr int device_type_rank[] = {
            0, // vk::PhysicalDeviceType::eOther
            3, // vk::PhysicalDeviceType::eIntegratedGpu
            4, // vk::PhysicalDeviceType::eDiscreteGpu
            2, // vk::PhysicalDeviceType::eVirtualGpu
            1, // vk::PhysicalDeviceType::eCpu
        };

        if (lhs.second.deviceType != rhs.second.deviceType) {
            return device_type_rank[static_cast<std::underlying_type_t<vk::PhysicalDeviceType>>(lhs.second.deviceType)]
                < device_type_rank[static_cast<std::underlying_type_t<vk::PhysicalDeviceType>>(rhs.second.deviceType)];
        }
        return lhs.second.limits.maxImageDimension2D < rhs.second.limits.maxImageDimension2D;
    }

    decltype(auto) simple_queue_filter(vk::QueueFlagBits required_flags, uint32_t min_queue_count = 1) {
        return [required_flags, min_queue_count](uint32_t queue_index, const vk::QueueFamilyProperties& queue_property){ return (queue_property.queueFlags & required_flags) && (queue_property.queueCount >= min_queue_count); };
    }
}

#endif