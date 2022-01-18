#ifndef TIARA_CORE_UTILITIES_PREDICATES_COMBINATORS
#define TIARA_CORE_UTILITIES_PREDICATES_COMBINATORS

#include "tiara/core/stdincludes.hpp"
#include "tiara/core/utilities/concept_invocable_r.hpp"

#include <tuple>
#include <utility>

namespace tiara::core::utils::preds {
    template <typename... Args>
    struct combinators {
        /**
         *  @brief higher order predicate object which its result is the result of its member and'ed together
         */
        template <typename... Preds> requires (InvocableR<Preds, bool, Args...> && ...)
        struct and_ {
            template <typename... UPreds>
            requires std::constructible_from<std::tuple<Preds...>, UPreds...> && (sizeof...(UPreds) > 1)
            constexpr and_(UPreds&&... upreds) noexcept(std::is_nothrow_constructible_v<std::tuple<Preds...>, UPreds...>) : preds{std::forward<UPreds>(upreds)...} {}

            template <typename UPred>
            requires std::constructible_from<std::tuple<Preds...>, UPred> && std::convertible_to<UPred, std::tuple<Preds...>>
            constexpr and_(UPred&& upred) noexcept(std::is_nothrow_constructible_v<std::tuple<Preds...>, UPred>): preds{std::forward<UPred>(upred)} {}

            template <typename UPred>
            requires std::constructible_from<std::tuple<Preds...>, UPred> && (!std::convertible_to<UPred, std::tuple<Preds...>>)
            explicit constexpr and_(UPred&& upred) noexcept(std::is_nothrow_constructible_v<std::tuple<Preds...>, UPred>): preds{std::forward<UPred>(upred)} {}

            constexpr bool operator()(Args... args) {
                return std::apply(
                    [&args...](const Preds&... pred_refs){
                        return (pred_refs(std::forward<Args>(args)...) && ...);
                    },
                    preds
                );
            }

            private:
            std::tuple<Preds...> preds;
        };

        #if TIARA_DETAILS_USE_NESTED_DEDUCTION_GUIDES_17
        template <typename... UPreds>
        and_(UPreds&&...) -> and_<std::decay_t<UPreds>...>;
        #endif

        /**
         *  @brief make and_, similar to pre c++17 std::make_... due to CTAD being buggy for nested class in gcc<12 (gcc#79501)
         */
        template <typename... UPreds>
        static constexpr and_<std::decay_t<UPreds>...> make_and_(UPreds&&... upreds) noexcept(std::is_nothrow_constructible_v<and_<std::decay_t<UPreds>...>, UPreds...>) {
            return { std::forward<UPreds>(upreds)... };
        }

        /**
         *  @brief higher order predicate object which its result is the result of its member or'ed together
         */
        template <typename... Preds> requires (InvocableR<Preds, bool, Args...> && ...)
        struct or_ {
            template <typename... UPreds>
            requires std::constructible_from<std::tuple<Preds...>, UPreds...> && (sizeof...(UPreds) > 1)
            constexpr or_(UPreds&&... upreds): preds{std::forward<UPreds>(upreds)...} {}

            template <typename UPred>
            requires std::constructible_from<std::tuple<Preds...>, UPred> && std::convertible_to<UPred, std::tuple<Preds...>>
            constexpr or_(UPred&& upred): preds{std::forward<UPred>(upred)} {}

            template <typename UPred>
            requires std::constructible_from<std::tuple<Preds...>, UPred> && (!std::convertible_to<UPred, std::tuple<Preds...>>)
            explicit constexpr or_(UPred&& upred): preds{std::forward<UPred>(upred)} {}

            constexpr bool operator()(Args... args) {
                return std::apply(
                    [&args...](const Preds&... pred_refs){
                        return (pred_refs(std::forward<Args>(args)...) || ...);
                    },
                    preds
                );
            }

            private:
            std::tuple<Preds...> preds;
        };

        #if TIARA_DETAILS_USE_NESTED_DEDUCTION_GUIDES_17
        template <typename... UPreds>
        or_(UPreds&&...) -> or_<std::decay_t<UPreds>...>;
        #endif

        /**
         *  @brief make or_, similar to pre c++17 std::make_... due to CTAD being buggy for nested class in gcc<12 (gcc#79501)
         */
        template <typename... UPreds>
        static constexpr or_<std::decay_t<UPreds>...> make_or_(UPreds&&... upreds) noexcept(std::is_nothrow_constructible_v<and_<std::decay_t<UPreds>...>, UPreds...>) {
            return { std::forward<UPreds>(upreds)... };
        }
    };
}

#endif
