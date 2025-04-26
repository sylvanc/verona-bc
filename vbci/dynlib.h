#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace vbci
{
#ifdef _WIN32
  using LibHandle = HMODULE;
#else
  using LibHandle = void*;
#endif

  struct Dynlib
  {
  private:
    LibHandle handle = nullptr;

  public:
    Dynlib(const std::string& path)
    {
#ifdef _WIN32
      if (path.empty())
        handle = GetModuleHandleA(nullptr);
      else
        handle = LoadLibraryA(path.c_str());
#else
      if (path.empty())
        handle = dlopen(nullptr, RTLD_LOCAL | RTLD_NOW);
      else
        handle = dlopen(path.c_str(), RTLD_LOCAL | RTLD_NOW);
#endif
    }

    ~Dynlib()
    {
      if (!handle)
        return;

#ifdef _WIN32
      FreeLibrary(handle);
#else
      dlclose(handle);
#endif
    }

    void* symbol(const std::string& name) const
    {
      if (!handle)
        return nullptr;

#ifdef _WIN32
      return reinterpret_cast<void*>(GetProcAddress(handle, name.c_str()));
#else
      return dlsym(handle, name.c_str());
#endif
    }
  };
}
