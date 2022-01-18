#ifndef TIARA_CORE_STDINCLUDES
#define TIARA_CORE_STDINCLUDES

#include "boost/predef.h"

#define VULKAN_HPP_NO_CONSTRUCTORS
#include "vulkan/vulkan_raii.hpp"
#include "GLFW/glfw3.h"

#include <version>

#if defined(__cpp_lib_ranges) && !defined(TIARA_DETAILS_USE_STD_RANGES_20)
#define TIARA_DETAILS_USE_STD_RANGES_20 1
#else
#define TIARA_DETAILS_USE_STD_RANGES_20 0
#endif

#if defined(__cpp_deduction_guides) && !defined(TIARA_DETAILS_USE_DEDUCTION_GUIDES_17)
#define TIARA_DETAILS_USE_DEDUCTION_GUIDES_17 1
#else
#define TIARA_DETAILS_USE_DEDUCTION_GUIDES_17 0
#endif

#if (((BOOST_COMP_GNUC && (__GNUC__ >= 12)) || !BOOST_COMP_GNUC) && defined(TIARA_DETAILS_USE_DEDUCTION_GUIDES_17)) && !defined(TIARA_DETAILS_USE_NESTED_DEDUCTION_GUIDES_17)
#define TIARA_DETAILS_USE_NESTED_DEDUCTION_GUIDES_17 1
#else
#define TIARA_DETAILS_USE_NESTED_DEDUCTION_GUIDES_17 0
#endif

#ifndef __has_feature
#define TIARA_COMPILER_HAS_FEATURE(x) 0
#else
#define TIARA_COMPILER_HAS_FEATURE __has_feature
#endif

#if TIARA_COMPILER_HAS_FEATURE(address_sanitizer) || defined(__SANITIZE_ADDRESS__) 
#include <sanitizer/lsan_interface.h>
#define TIARA_SUPPRESS_LSAN_SCOPE ::__lsan::ScopedDisabler _lsan_scope{};
#else
#define TIARA_SUPPRESS_LSAN_SCOPE
#endif

#endif
