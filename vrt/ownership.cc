#include "ownership.h"

#include "failure.h"
#include "header.h"
#include "thread_context.h"

namespace vrt::ownership
{
  void retain_root(Value value)
  {
    if (!is_unmanaged_value_type(value.type()))
      value.header()->root_ref_inc();
  }

  void release_root(Value value)
  {
    if (!is_unmanaged_value_type(value.type()))
      value.header()->root_ref_dec();
  }

  void retain_field(Value value)
  {
    if (!is_unmanaged_value_type(value.type()))
      value.header()->field_ref_inc();
  }

  void release_field(Value value)
  {
    if (!is_unmanaged_value_type(value.type()))
      value.header()->field_ref_dec();
  }

  void escape(Value value)
  {
    if (is_unmanaged_value_type(value.type()))
      return;

    auto* header = value.header();
    if (header->location().is_immortal())
      return;

    auto* context = ThreadContext::try_get();
    internal_check(context != nullptr, Failure::invalid_header_state);
    context->escape(header);
  }
}
