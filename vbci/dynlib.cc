#include "dynlib.h"

namespace vbci
{
  ValueType platform_type(ValueType t)
  {
    switch (t)
    {
      case ValueType::ILong:
        return sizeof(long) == 4 ? ValueType::I32 : ValueType::I64;

      case ValueType::ULong:
        return sizeof(unsigned long) == 4 ? ValueType::U32 : ValueType::U64;

      case ValueType::ISize:
        return sizeof(ssize_t) == 4 ? ValueType::I32 : ValueType::I64;

      case ValueType::USize:
        return sizeof(size_t) == 4 ? ValueType::U32 : ValueType::U64;

      case ValueType::Ptr:
        return sizeof(void*) == 4 ? ValueType::U32 : ValueType::U64;

      default:
        return t;
    }
  }

  std::pair<ValueType, ffi_type*> map(Id type_id)
  {
    if (type_id & TypeArray)
      return {ValueType::None, nullptr};

    auto t = static_cast<ValueType>(type_id >> TypeShift);

    switch (t)
    {
      case ValueType::None:
        return {t, &ffi_type_void};
      case ValueType::I8:
        return {t, &ffi_type_sint8};
      case ValueType::I16:
        return {t, &ffi_type_sint16};
      case ValueType::I32:
        return {t, &ffi_type_sint32};
      case ValueType::I64:
        return {t, &ffi_type_sint64};
      case ValueType::U8:
        return {t, &ffi_type_uint8};
      case ValueType::U16:
        return {t, &ffi_type_uint16};
      case ValueType::U32:
        return {t, &ffi_type_uint32};
      case ValueType::U64:
        return {t, &ffi_type_uint64};
      case ValueType::F32:
        return {t, &ffi_type_float};
      case ValueType::F64:
        return {t, &ffi_type_double};
      case ValueType::ILong:
        return {platform_type(t), &ffi_type_slong};
      case ValueType::ULong:
        return {platform_type(t), &ffi_type_ulong};
      case ValueType::ISize:
        return {
          platform_type(t),
          sizeof(ssize_t) == 4 ? &ffi_type_sint32 : &ffi_type_sint64};
      case ValueType::USize:
        return {
          platform_type(t),
          sizeof(size_t) == 4 ? &ffi_type_uint32 : &ffi_type_uint64};
      case ValueType::Ptr:
        return {t, &ffi_type_pointer};
      default:
        return {t, nullptr};
    }
  }

  Symbol::Symbol(Func func) : func(func), return_type(nullptr) {}

  bool Symbol::param(Id type_id)
  {
    auto [vt, ffit] = map(type_id);

    if (!ffit)
      return false;

    param_types.push_back(ffit);
    param_vtypes.push_back(vt);
    return true;
  }

  bool Symbol::ret(Id type_id)
  {
    auto [vt, ffit] = map(type_id);

    if (!ffit)
      return false;

    return_type = ffit;
    return_vtype = vt;
    return true;
  }

  bool Symbol::prepare()
  {
    if (!func)
      return false;

    return ffi_prep_cif(
             &cif,
             FFI_DEFAULT_ABI,
             static_cast<unsigned>(param_types.size()),
             return_type,
             param_types.data()) == FFI_OK;
  }

  const std::vector<ValueType>& Symbol::params()
  {
    return param_vtypes;
  }

  ValueType Symbol::ret()
  {
    return return_vtype;
  }

  uint64_t Symbol::call(std::vector<void*>& args)
  {
    if (!func)
      return 0;

    uint64_t ret;
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

  Symbol::Func Dynlib::symbol(const std::string& name)
  {
    if (!handle)
      return nullptr;

#ifdef _WIN32
    auto f = GetProcAddress(handle, name.c_str());
#else
    auto f = dlsym(handle, name.c_str());
#endif
    return reinterpret_cast<Symbol::Func>(f);
  }
}
