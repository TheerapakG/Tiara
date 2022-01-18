#ifndef TIARA_CORE_UTILITIES_IS_TUPLE
#define TIARA_CORE_UTILITIES_IS_TUPLE

#include <tuple>

namespace tiara::core::utils {
    template <typename T> struct is_tuple: std::false_type {};
    template <typename... Ts> struct is_tuple<std::tuple<Ts...>>: std::true_type {};

    template <typename T>
    concept TupleType = is_tuple<T>::value;
}

#endif
