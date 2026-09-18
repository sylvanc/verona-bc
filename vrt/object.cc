#include "object.h"

#include "failure.h"
#include "program.h"

#include <cstdint>
#include <new>

namespace vrt
{
  Object::Object(std::byte* allocation, const Class* cls)
  : Header(Location::immortal(), cls->id), cls(cls)
  {
    this->allocation = allocation;
  }

  Object* Object::create_singleton(std::byte* storage, const Class* cls)
  {
    if (
      (storage == nullptr) ||
      ((reinterpret_cast<uintptr_t>(storage) % alignof(Object)) != 0))
      fail(Failure::invalid_object_state);

    return ::new (storage) Object{storage, cls};
  }

  void init_singleton(void* storage, const Class* cls)
  {
    if (
      (cls == nullptr) || (cls->field_count != 0) ||
      (cls->payload_size != 0) || (cls->singleton == nullptr) ||
      ((cls->method_count != 0) && (cls->methods == nullptr)))
      fail(Failure::invalid_object_state);

    if (layout_type_id(cls->id).value_type != ValueType::object)
      fail(Failure::invalid_object_state);

    auto* bytes = static_cast<std::byte*>(storage);
    if (cls->singleton != (bytes + Object::singleton_payload_offset()))
      fail(Failure::invalid_object_state);

    auto* object = Object::create_singleton(bytes, cls);
    if (object->get_payload() != cls->singleton)
      fail(Failure::invalid_object_state);
  }
}
