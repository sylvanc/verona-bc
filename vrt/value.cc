#include "value.h"

#include "error.h"
#include "header.h"

namespace vrt
{
  Header* Value::header() const
  {
    return Header::from_data(value_type, data_address);
  }

  Location Value::location() const
  {
    if (is_unmanaged_value_type(value_type))
      return Location::immortal();

    return header()->location();
  }

  Region* Value::region() const
  {
    if ((value_type == ValueType::object) || (value_type == ValueType::array))
    {
      auto* result = header()->region();
      if (result != nullptr)
        return result;
    }

    raise_error(Error::bad_alloc_target);
  }
}
