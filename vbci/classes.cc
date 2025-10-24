#include "classes.h"

#include "object.h"

namespace vbci
{
  bool Class::calc_size()
  {
    if (fields.empty())
    {
      // Create an immortal singleton object for empty classes.
      size = sizeof(Object);
      auto mem = new uint8_t[size];
      singleton = Object::create(mem, *this, loc::Immortal);
      return true;
    }

    singleton = nullptr;
    auto program = Program::get();
    std::vector<ffi_type*> ffi_types;

    for (auto& f : fields)
    {
      auto rep = program.layout_type_id(f.type_id);
      f.value_type = rep.first;
      ffi_types.push_back(rep.second);
    }

    ffi_types.push_back(nullptr);

    std::vector<size_t> field_offsets;
    field_offsets.resize(fields.size());

    ffi_type struct_type;
    struct_type.size = 0;
    struct_type.alignment = 0;
    struct_type.type = FFI_TYPE_STRUCT;
    struct_type.elements = ffi_types.data();

    if (
      ffi_get_struct_offsets(
        FFI_DEFAULT_ABI, &struct_type, field_offsets.data()) != FFI_OK)
      return false;

    size = sizeof(Object) + struct_type.size;

    for (size_t i = 0; i < fields.size(); i++)
    {
      fields.at(i).offset = field_offsets.at(i);
      fields.at(i).size = ffi_types.at(i)->size;
    }

    return true;
  }

  Function* Class::finalizer()
  {
    return method(FinalMethodId);
  }

  Function* Class::method(size_t w)
  {
    auto find = methods.find(w);
    if (find == methods.end())
      return nullptr;

    return find->second;
  }

  Class::~Class()
  {
    if (singleton)
    {
      // Don't finalize the singleton objects, but do collect the
      // memory to appease LSAN.
      delete[] reinterpret_cast<uint8_t*>(singleton);
    }
  }
}
