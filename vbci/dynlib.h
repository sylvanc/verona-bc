#pragma once

#include "platform.h"
#include "value.h"

#include <ffi.h>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vbci.h>
#include <vector>

#if !defined(PLATFORM_IS_WINDOWS)
#  include <dlfcn.h>
#endif

namespace vbci
{
#if defined(PLATFORM_IS_WINDOWS)
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

    std::vector<TypeId> param_types;
    std::vector<ValueType> param_value_types;
    TypeId return_type;
    ValueType return_value_type;

    std::vector<ffi_type*> param_ffi_types;
    ffi_type* return_ffi_type;

  public:
    Symbol(Func func, bool vararg);

    void param(TypeId type_id);
    void ret(TypeId type_id);
    bool prepare();

    bool varargs();
    std::vector<TypeId>& params();
    std::vector<ValueType>& paramvals();
    TypeId ret();
    ValueType retval();

    void varparam(ffi_type* ffit);
    Value call(std::vector<void*>& args);
    void* raw_pointer();
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
