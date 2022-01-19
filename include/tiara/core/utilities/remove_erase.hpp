#ifndef TIARA_CORE_UTILITIES_REMOVE_ERASE
#define TIARA_CORE_UTILITIES_REMOVE_ERASE

#include "tiara/core/stdincludes.hpp"

#include <algorithm>

namespace tiara::core::utils {
    template <typename R, typename T>
    #if TIARA_DETAILS_USE_STD_RANGES_20
    requires std::ranges::forward_range<R> &&
        std::permutable<std::ranges::iterator_t<R>> &&
        std::indirect_binary_predicate<std::ranges::equal_to, std::ranges::iterator_t<R>, const T*>
    #endif
    constexpr void remove_erase(R& r, const T& value) {
    #if TIARA_DETAILS_USE_STD_RANGES_20
        const auto [first, last] = std::ranges::remove(r, value);
        r.erase(first, last);
    #else
        const auto last = std::remove(r.begin(), r.end(), value);
        r.erase(last, r.end());
    #endif
    }

    template <typename R, typename Pred>
    #if TIARA_DETAILS_USE_STD_RANGES_20
    requires std::ranges::forward_range<R> &&
        std::permutable<std::ranges::iterator_t<R>> &&
        std::indirect_unary_predicate<Pred, std::ranges::iterator_t<R>>
    #endif
    constexpr void remove_erase_if(R& r, Pred pred) {
    #if TIARA_DETAILS_USE_STD_RANGES_20
        const auto [first, last] = std::ranges::remove_if(r, pred);
        r.erase(first, last);
    #else
        const auto last = std::remove_if(r.begin(), r.end(), pred);
        r.erase(last, r.end());
    #endif
    }
}

#endif
