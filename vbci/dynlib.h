#pragma once

#include <ffi.h>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vbci.h>
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

  struct Symbol
  {
    using Func = void (*)();

  private:
    ffi_cif cif;
    Func func;
    bool vararg;

    std::vector<Id> param_types;
    Id return_type;
    ValueType return_value_type;

    std::vector<ffi_type*> param_ffi_types;
    ffi_type* return_ffi_type;

  public:
    Symbol(Func func, bool vararg);

    void param(Id type_id, ffi_type* t);
    void ret(Id type_id, ValueType t, ffi_type* ffit);
    bool prepare();

    bool varargs();
    std::vector<Id>& params();
    Id ret();
    ValueType retval();

    void varparam(ffi_type* ffit);
    uint64_t call(std::vector<void*>& args);
  };

  struct Dynlib
  {
  private:
    LibHandle handle = nullptr;

  public:
    Dynlib(const std::string& path);
    ~Dynlib();

    Symbol::Func symbol(const std::string& name, const std::string& version);
  };
}
