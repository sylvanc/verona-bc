#pragma once

#include <vbci.h>

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
