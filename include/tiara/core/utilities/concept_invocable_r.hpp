#ifndef TIARA_CORE_UTILITIES_CONCEPT_INVOCABLE_R
#define TIARA_CORE_UTILITIES_CONCEPT_INVOCABLE_R

#include <concepts>
#include <type_traits>

namespace tiara::core::utils {
    template <typename F, typename R, typename... Args>
    concept InvocableR = std::invocable<F&&, Args&&...> && std::same_as<R, std::invoke_result_t<F&&, Args&&...>>;
}

#endif
