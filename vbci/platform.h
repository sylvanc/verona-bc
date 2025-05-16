#pragma once

#if defined(__APPLE__) && defined(__MACH__)
#  define PLATFORM_IS_MACOSX
#elif defined(__linux__)
#  define PLATFORM_IS_LINUX
#elif defined(_WIN32)
#  define PLATFORM_IS_WINDOWS
#  include <windows.h>
#endif
