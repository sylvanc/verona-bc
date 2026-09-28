#include "ownership.h"

#include "error.h"
#include "failure.h"
#include "frame.h"
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

  void validate_tailcall(Value value)
  {
    if (is_unmanaged_value_type(value.type()))
      return;

    auto* context = ThreadContext::try_get();
    internal_check(
      (context != nullptr) && (context->thread.frame != nullptr),
      Failure::invalid_frame_state);

    auto* header = value.header();
    if (
      header->location().is_stack() &&
      (header->location() == context->thread.frame->frame_id))
      raise_error(Error::bad_stack_escape);
  }
}
