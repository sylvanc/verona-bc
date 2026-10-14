#include "header.h"

#include "array.h"
#include "collect.h"
#include "failure.h"
#include "object.h"
#include "program.h"
#include "region.h"
#include "value.h"

#include <limits>

namespace vrt
{
  ValueType Header::value_type() const
  {
    return layout_type_id(type_id).value_type;
  }

  Header* Header::representative()
  {
    return const_cast<Header*>(
      static_cast<const Header*>(this)->representative());
  }

  const Header* Header::representative() const
  {
    if (!loc.is_scc_ptr())
      return this;

    auto* result = loc.scc_target();
    internal_check(
      (result != this) && (result->location() == Location::immutable()),
      Failure::invalid_header_state);
    return result;
  }

  Header* Header::find(Header* header)
  {
    internal_check(header != nullptr, Failure::invalid_header_state);
    if (!header->loc.is_scc_ptr())
      return header;

    auto* target = header->loc.scc_target();
    auto* root = find(target);
    if (root != target)
      header->loc = Location::scc_ptr(root);

    return root;
  }

  bool Header::try_begin_scc_collection()
  {
    bool expected = false;
    return collecting_scc.compare_exchange_strong(
      expected, true, std::memory_order_acq_rel, std::memory_order_relaxed);
  }

  bool Header::is_collecting_scc() const
  {
    return collecting_scc.load(std::memory_order_acquire);
  }

  RC Header::get_rc() const
  {
    return reference_count;
  }

  void Header::set_rc(RC value)
  {
    reference_count = value;
  }

  RC Header::get_arc() const
  {
    auto& count = const_cast<RC&>(reference_count);
    return ARC{count}.load(std::memory_order_relaxed);
  }

  void Header::set_arc(RC value)
  {
    ARC{reference_count}.store(value, std::memory_order_relaxed);
  }

  void Header::inc_arc()
  {
    auto count = ARC{reference_count};
    auto current = count.load(std::memory_order_relaxed);

    do
    {
      internal_check(
        current != std::numeric_limits<RC>::max(),
        Failure::invalid_header_state);
    } while (!count.compare_exchange_weak(
      current,
      current + 1,
      std::memory_order_relaxed,
      std::memory_order_relaxed));
  }

  bool Header::dec_arc()
  {
    auto count = ARC{reference_count};
    auto current = count.load(std::memory_order_relaxed);

    do
    {
      internal_check(current != 0, Failure::invalid_header_state);
    } while (!count.compare_exchange_weak(
      current,
      current - 1,
      std::memory_order_acq_rel,
      std::memory_order_relaxed));

    return current == 1;
  }

  Header* Header::from_data(ValueType value_type, const void* data_address)
  {
    internal_check(data_address != nullptr, Failure::invalid_value_state);

    Header* result = nullptr;
    switch (value_type)
    {
      case ValueType::object:
        result = reinterpret_cast<Object*>(const_cast<void*>(data_address)) - 1;
        break;

      case ValueType::array:
        result = reinterpret_cast<Array*>(const_cast<void*>(data_address)) - 1;
        break;

      default:
        fail(Failure::invalid_value_state);
    }

    internal_check(
      (result->magic == Header::magic_value) &&
        (result->value_type() == value_type) &&
        (result->data() == data_address),
      Failure::invalid_value_state);

    return result;
  }

  void* Header::data()
  {
    return const_cast<void*>(static_cast<const Header*>(this)->data());
  }

  const void* Header::data() const
  {
    internal_check(magic == Header::magic_value, Failure::invalid_header_state);

    switch (value_type())
    {
      case ValueType::object:
        return static_cast<const Object*>(this)->fields();

      case ValueType::array:
        return static_cast<const Array*>(this)->elements();

      default:
        fail(Failure::invalid_header_state);
    }
  }

  void Header::root_ref_inc()
  {
    field_ref_inc();
    if (loc.is_region())
      loc.to_region()->stack_inc();
  }

  void Header::root_ref_dec()
  {
    if (loc.is_region() && !loc.to_region()->stack_dec())
      return;

    field_ref_dec();
  }

  void Header::field_ref_inc()
  {
    if (loc.is_scc_ptr())
    {
      representative()->field_ref_inc();
      return;
    }

    if (loc.is_stack() || loc.is_immortal())
      return;

    if (loc.is_immutable())
    {
      internal_check(!is_collecting_scc(), Failure::invalid_header_state);
      inc_arc();
      return;
    }

    auto* region = this->region();
    if (
      (region == nullptr) || region->destroying || region->is_finalizing() ||
      region->is_arena())
      return;

    internal_check(
      reference_count != std::numeric_limits<uintptr_t>::max(),
      Failure::invalid_header_state);

    reference_count++;
  }

  void Header::field_ref_dec()
  {
    if (loc.is_scc_ptr())
    {
      representative()->field_ref_dec();
      return;
    }

    if (loc.is_stack() || loc.is_immortal())
      return;

    if (loc.is_immutable())
    {
      if (is_collecting_scc())
        return;

      if (dec_arc())
        collect_scc(this);

      return;
    }

    auto* region = this->region();
    if (
      (region == nullptr) || region->destroying || region->is_finalizing() ||
      region->is_arena())
      return;

    internal_check(reference_count != 0, Failure::invalid_header_state);

    reference_count--;
    if (reference_count == 0)
      collect(this);
  }

  void Header::finalize()
  {
    switch (value_type())
    {
      case ValueType::object:
        static_cast<Object*>(this)->finalize();
        return;

      case ValueType::array:
        static_cast<Array*>(this)->finalize();
        return;

      default:
        fail(Failure::invalid_header_state);
    }
  }

  void Header::destroy_storage()
  {
    switch (value_type())
    {
      case ValueType::object:
        static_cast<Object*>(this)->destroy_storage();
        return;

      case ValueType::array:
        static_cast<Array*>(this)->destroy_storage();
        return;

      default:
        fail(Failure::invalid_header_state);
    }
  }

}
