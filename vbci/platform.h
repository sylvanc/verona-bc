#pragma once

#if defined(__APPLE__) && defined(__MACH__)
#  define PLATFORM_IS_MACOSX
#elif defined(__linux__)
#  define PLATFORM_IS_LINUX
#elif defined(_WIN32)
#  define PLATFORM_IS_WINDOWS
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#endif

#if defined(PLATFORM_IS_WINDOWS)
#  define VBCI_KEEP __declspec(dllexport)
#else
#  define VBCI_KEEP [[gnu::used]] [[gnu::retain]]
#endif

#define VBCI_FFI extern "C" VBCI_KEEP

namespace vbci::platform
{
  constexpr inline bool is_mac()
  {
#if defined(PLATFORM_IS_MACOSX)
    return true;
#else
    return false;
#endif
  }

  constexpr inline bool is_linux()
  {
#if defined(PLATFORM_IS_LINUX)
    return true;
#else
    return false;
#endif
  }

  constexpr inline bool is_windows()
  {
#if defined(PLATFORM_IS_WINDOWS)
    return true;
#else
    return false;
#endif
  }
}
