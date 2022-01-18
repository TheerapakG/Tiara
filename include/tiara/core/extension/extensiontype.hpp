#ifndef TIARA_CORE_EXTENSION_EXTENSIONTYPE
#define TIARA_CORE_EXTENSION_EXTENSIONTYPE

#include "tiara/core/utilities/is_tuple.hpp"

#include <concepts>
#include <optional>

namespace tiara::core::extension {
    struct ExtensionBase {
        ExtensionBase() = default;
        ExtensionBase(const ExtensionBase&) = delete;
        ExtensionBase(ExtensionBase&&) = default;

        ExtensionBase& operator=(const ExtensionBase&) = delete;
        ExtensionBase& operator=(ExtensionBase&&) = default;

        virtual ~ExtensionBase() = default;

        virtual void init() = 0;
        virtual void deinit() = 0;
    };
}

namespace tiara::core::extension::detail {
    class ExtensionInitHandle {
        public:
        ExtensionInitHandle(std::unique_ptr<ExtensionBase> ext): _ext{std::move(ext)} {
            if(_ext) _ext->init();
        }

        ExtensionInitHandle(const ExtensionInitHandle&) = delete;
        ExtensionInitHandle(ExtensionInitHandle&&) = default;

        ExtensionInitHandle& operator=(const ExtensionInitHandle&) = delete;
        ExtensionInitHandle& operator=(ExtensionInitHandle&&) = default;

        virtual ~ExtensionInitHandle() {
            if (_ext) _ext->deinit();
        }

        private:
        std::unique_ptr<ExtensionBase> _ext;
    };
}

namespace tiara::core::extension {
    template <typename Ext /* CRTP */>
    struct Extension: public ExtensionBase {
        static std::optional<detail::ExtensionInitHandle> init_ext() {
            static_assert(std::same_as<decltype(Ext::is_init()), bool>, "no suitable is_init implemented for Ext");
            if (Ext::is_init()) return std::nullopt;
            return detail::ExtensionInitHandle{std::unique_ptr<ExtensionBase>(new Ext{})};
        }
    };
}

#endif
