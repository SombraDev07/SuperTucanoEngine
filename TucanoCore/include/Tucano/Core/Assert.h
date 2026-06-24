#pragma once

#include "Tucano/Core/Logger.h"

// Break macro for MSVC and GCC/Clang
#ifdef _MSC_VER
    #define TUCANO_DEBUGBREAK() __debugbreak()
#else
    #define TUCANO_DEBUGBREAK() __builtin_trap()
#endif

#if defined(_DEBUG) || defined(DEBUG)
    // Core assert
    #define TUCANO_CORE_ASSERT(x, ...) { if(!(x)) { TUCANO_CORE_CRITICAL("Assertion Failed: {0}", __VA_ARGS__); TUCANO_DEBUGBREAK(); } }
    // Client assert
    #define TUCANO_ASSERT(x, ...) { if(!(x)) { TUCANO_CRITICAL("Assertion Failed: {0}", __VA_ARGS__); TUCANO_DEBUGBREAK(); } }
#else
    #define TUCANO_CORE_ASSERT(x, ...)
    #define TUCANO_ASSERT(x, ...)
#endif
