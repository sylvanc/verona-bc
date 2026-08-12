#pragma once

#if defined(_WIN32)
#  define VRT_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#  define VRT_EXPORT __attribute__((visibility("default")))
#else
#  define VRT_EXPORT
#endif
