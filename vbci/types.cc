#include "types.h"

#include "object.h"

namespace vbci
{
  bool Class::calc_size(std::vector<ffi_type*>& ffi_types)
  {
    std::vector<size_t> field_offsets;
    field_offsets.resize(field_map.size());

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
}
