#include "dynlib.h"

namespace vbci
{
  ffi_type* ffi_map(Id type_id)
  {
    if (!type::is_val(type_id))
      return nullptr;

    auto t = type::val(type_id);

    switch (t)
    {
      case ValueType::None:
        return &ffi_type_void;
      case ValueType::I8:
        return &ffi_type_sint8;
      case ValueType::I16:
        return &ffi_type_sint16;
      case ValueType::I32:
        return &ffi_type_sint32;
      case ValueType::I64:
        return &ffi_type_sint64;
      case ValueType::U8:
        return &ffi_type_uint8;
      case ValueType::U16:
        return &ffi_type_uint16;
      case ValueType::U32:
        return &ffi_type_uint32;
      case ValueType::U64:
        return &ffi_type_uint64;
      case ValueType::F32:
        return &ffi_type_float;
      case ValueType::F64:
        return &ffi_type_double;
      case ValueType::ILong:
        return &ffi_type_slong;
      case ValueType::ULong:
        return &ffi_type_ulong;
      case ValueType::ISize:
        return sizeof(ssize_t) == 4 ? &ffi_type_sint32 : &ffi_type_sint64;
      case ValueType::USize:
        return sizeof(size_t) == 4 ? &ffi_type_uint32 : &ffi_type_uint64;
      case ValueType::Ptr:
        return &ffi_type_pointer;
      default:
        return nullptr;
    }
  }

  Symbol::Symbol(Func func) : func(func), return_ffi_type(nullptr) {}

  void Symbol::param(Id type_id, ffi_type* ffit)
  {
    param_types.push_back(type_id);
    param_ffi_types.push_back(ffit);
  }

  void Symbol::ret(Id type_id, ValueType t, ffi_type* ffit)
  {
    return_type = type_id;
    return_value_type = t;
    return_ffi_type = ffit;
  }

  bool Symbol::prepare()
  {
    if (!func)
      return false;

    return ffi_prep_cif(
             &cif,
             FFI_DEFAULT_ABI,
             static_cast<unsigned>(param_ffi_types.size()),
             return_ffi_type,
             param_ffi_types.data()) == FFI_OK;
  }

  std::vector<Id>& Symbol::params()
  {
    return param_types;
  }

  Id Symbol::ret()
  {
    return return_type;
  }

  ValueType Symbol::retval()
  {
    return return_value_type;
  }

  uint64_t Symbol::call(std::vector<void*>& args)
  {
    if (!func)
      return 0;

    ffi_arg ret = 0;
    ffi_call(&cif, func, &ret, args.data());
    return ret;
  }

  Dynlib::Dynlib(const std::string& path)
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

  Dynlib::~Dynlib()
  {
    if (!handle)
      return;

#ifdef _WIN32
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

#ifdef _WIN32
    auto f = GetProcAddress(handle, name.c_str());
#else
    void* f;

    if (version.empty())
      f = dlsym(handle, name.c_str());
    else
      f = dlvsym(handle, name.c_str(), version.c_str());
#endif
    return reinterpret_cast<Symbol::Func>(f);
  }
}
