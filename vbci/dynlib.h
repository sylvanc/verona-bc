#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

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
    Dynlib(const std::filesystem::path& path)
    {
#ifdef _WIN32
      if (path.empty())
        handle = GetModuleHandleA(nullptr);
      else
        handle = LoadLibraryA(path.c_str());
#else
      if (path.empty())
        handle = dlopen(nullptr, RTLD_GLOBAL | RTLD_NOW);
      else
        handle = dlopen(path.c_str(), RTLD_GLOBAL | RTLD_NOW);
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

    void* symbol(const char* name) const
    {
      if (!handle)
        return nullptr;

#ifdef _WIN32
      return reinterpret_cast<void*>(GetProcAddress(handle, name));
#else
      return dlsym(handle, name);
#endif
    }
  };

  struct Dynlibs
  {
  private:
    std::unordered_map<std::filesystem::path, Dynlib> libs;

  public:
    Dynlib& get()
    {
      return get(std::filesystem::path{});
    }

    Dynlib& get(const std::filesystem::path& path)
    {
      auto find = libs.find(path);

      if (find != libs.end())
        return find->second;

      return libs.emplace(path, Dynlib(path)).first->second;
    }

    void clear()
    {
      libs.clear();
    }
  };
}
