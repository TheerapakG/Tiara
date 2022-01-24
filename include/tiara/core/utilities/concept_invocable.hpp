#ifndef TIARA_CORE_UTILITIES_CONCEPT_INVOCABLE
#define TIARA_CORE_UTILITIES_CONCEPT_INVOCABLE

#include <concepts>
#include <type_traits>

namespace tiara::core::utils {
    template <typename F, typename R, typename... Args>
    concept InvocableR = std::invocable<F&&, Args&&...> && std::same_as<R, std::invoke_result_t<F&&, Args&&...>>;

    template <typename F, typename C, typename... Args>
    concept InvocableC = std::invocable<F&&, Args&&...> && std::convertible_to<C, std::invoke_result_t<F&&, Args&&...>>;
}

#endif
