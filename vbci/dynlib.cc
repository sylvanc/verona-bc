#include "dynlib.h"

#include "program.h"
#include "value.h"

namespace vbci
{
  Symbol::Symbol(Func func, bool vararg) : func(func), vararg(vararg) {}

  void Symbol::param(uint32_t type_id)
  {
    param_types.push_back(type_id);
  }

  void Symbol::ret(uint32_t type_id)
  {
    return_type = type_id;
  }

  bool Symbol::prepare()
  {
    if (!func)
      return false;

    auto& program = Program::get();

    for (auto& param : param_types)
    {
      auto rep = program.layout_type_id(param);
      param_value_types.push_back(rep.first);
      param_ffi_types.push_back(rep.second);
    }

    auto rep = program.layout_type_id(return_type);
    return_value_type = rep.first;
    return_ffi_type = rep.second;

    if (return_value_type == ValueType::Invalid)
      return_ffi_type = program.value_type();

    if (vararg)
      return true;

    return ffi_prep_cif(
             &cif,
             FFI_DEFAULT_ABI,
             static_cast<unsigned>(param_ffi_types.size()),
             return_ffi_type,
             param_ffi_types.data()) == FFI_OK;
  }

  bool Symbol::varargs()
  {
    return vararg;
  }

  std::vector<uint32_t>& Symbol::params()
  {
    return param_types;
  }

  std::vector<ValueType>& Symbol::paramvals()
  {
    return param_value_types;
  }

  uint32_t Symbol::ret()
  {
    return return_type;
  }

  ValueType Symbol::retval()
  {
    return return_value_type;
  }

  void Symbol::varparam(ffi_type* ffit)
  {
    if (!vararg)
      throw Value(Error::BadArgs);

    param_ffi_types.push_back(ffit);
  }

  Value Symbol::call(std::vector<void*>& args)
  {
    if (!func)
      throw Value(Error::UnknownFFI);

    if (vararg)
    {
      if (args.size() != param_ffi_types.size())
        throw Value(Error::BadArgs);

      if (
        ffi_prep_cif_var(
          &cif,
          FFI_DEFAULT_ABI,
          static_cast<unsigned>(param_ffi_types.size()),
          static_cast<unsigned>(args.size()),
          return_ffi_type,
          param_ffi_types.data()) != FFI_OK)
      {
        throw Value(Error::BadArgs);
      }
    }

    // Check if it's ffi_type_value, use a Value. The C++ type is constructed
    // into this space.
    Value ret;

    if (return_value_type == ValueType::Invalid)
    {
      ffi_call(&cif, func, &ret, args.data());
    }
    else
    {
      ffi_arg r = 0;
      ffi_call(&cif, func, &r, args.data());
      ret = Value::from_ffi(return_value_type, r);
    }

    if (vararg)
      param_ffi_types.resize(param_types.size());

    return ret;
  }

  void* Symbol::raw_pointer()
  {
    return reinterpret_cast<void*>(func);
  }

  Dynlib::Dynlib(const std::string& path)
  {
#if defined(PLATFORM_IS_WINDOWS)
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

  Dynlib::~Dynlib()
  {
    if (!handle)
      return;

#if defined(PLATFORM_IS_WINDOWS)
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif
  }

  Symbol::Func
  Dynlib::symbol(const std::string& name, const std::string& version)
  {
    if (!handle)
      return nullptr;

#if defined(PLATFORM_IS_WINDOWS)
    (void)version;
    auto f = GetProcAddress(handle, name.c_str());
#elif defined(PLATFORM_IS_LINUX)
    void* f;

    if (version.empty())
      f = dlsym(handle, name.c_str());
    else
      f = dlvsym(handle, name.c_str(), version.c_str());
#else
    // No symbol versioning.
    (void)version;
    void* f = dlsym(handle, name.c_str());
#endif
    return reinterpret_cast<Symbol::Func>(f);
  }
}
