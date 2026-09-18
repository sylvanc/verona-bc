#include "value.h"

#include "error.h"
#include "failure.h"
#include "header.h"
#include "object.h"

namespace vrt
{
  namespace
  {
    bool is_unmanaged(ValueType value_type)
    {
      switch (value_type)
      {
        case ValueType::none:
        case ValueType::scalar:
        case ValueType::raw_pointer:
          return true;

        default:
          return false;
      }
    }
  }

  Header* Value::header() const
  {
    internal_check(payload != nullptr, Failure::invalid_value_state);

    Header* result = nullptr;
    switch (value_type)
    {
      case ValueType::object:
        result = reinterpret_cast<Object*>(const_cast<void*>(payload)) - 1;
        break;

      default:
        fail(Failure::invalid_value_state);
    }

    internal_check(
      (result->magic == Header::magic_value) &&
        (result->value_type() == value_type) &&
        (payload_from_header(result) == payload),
      Failure::invalid_value_state);

    return result;
  }

  Location Value::location() const
  {
    if (is_unmanaged(value_type))
      return Location::immortal();

    return header()->location();
  }

  Region* Value::region() const
  {
    if (value_type == ValueType::object)
    {
      auto* result = header()->region();
      if (result != nullptr)
        return result;
    }

    raise_error(Error::bad_alloc_target);
  }

  void Value::reg_inc() const
  {
    if (!is_unmanaged(value_type))
      header()->reg_inc();
  }

  void Value::reg_dec() const
  {
    if (!is_unmanaged(value_type))
      header()->reg_dec();
  }

  void Value::field_inc() const
  {
    if (!is_unmanaged(value_type))
      header()->field_inc();
  }

  void Value::field_dec() const
  {
    if (!is_unmanaged(value_type))
      header()->field_dec();
  }

  void Value::escape() const
  {
    escape_header(header());
  }

  void Value::prepare_raise() const
  {
    prepare_raise_header(header());
  }
}
