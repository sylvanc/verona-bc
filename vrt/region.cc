#include "region.h"

#include "failure.h"
#include "frame.h"
#include "header.h"
#include "thread_context.h"
#include "region_arena.h"
#include "region_rc.h"

#include <limits>
#include <new>

namespace vrt
{
  Region* Region::create(RegionType type, uintptr_t frame_depth)
  {
    Region* region = nullptr;
    switch (type)
    {
      case RegionType::rc:
        region = new (std::nothrow) RegionRC{type, frame_depth};
        break;

      case RegionType::arena:
        region = new (std::nothrow) RegionArena{type, frame_depth};
        break;

      default:
        fail(Failure::invalid_region_state);
    }

    if (region == nullptr)
      fail(Failure::out_of_memory);

    return region;
  }

  bool Region::is_frame_local() const
  {
    return frame_depth != 0;
  }

  bool Region::is_arena() const
  {
    return false;
  }

  bool Region::has_parent() const
  {
    return parent != nullptr;
  }

  bool Region::is_ancestor_of(const Region* other) const
  {
    while (other != nullptr)
    {
      other = other->parent;
      if (other == this)
        return true;
    }

    return false;
  }

  void Region::stack_inc(uintptr_t amount)
  {
    if ((amount == 0) || is_frame_local())
      return;

    internal_check(
      !destroying &&
        (amount <=
         (std::numeric_limits<uintptr_t>::max() - stack_reference_count)),
      Failure::invalid_region_state);

    if ((stack_reference_count == 0) && (parent != nullptr))
      parent->stack_inc();

    stack_reference_count += amount;
  }

  bool Region::stack_dec(uintptr_t amount)
  {
    if ((amount == 0) || is_frame_local())
      return true;

    if (destroying)
      return false;

    internal_check(
      amount <= stack_reference_count, Failure::invalid_region_state);

    stack_reference_count -= amount;
    if (stack_reference_count != 0)
      return true;

    if (parent != nullptr)
      return parent->stack_dec();

    destroy_region(this);
    return false;
  }

  void Region::set_parent(Region* new_parent, Header* entry)
  {
    internal_check(
      (new_parent != nullptr) && (entry != nullptr) && (parent == nullptr) &&
        (new_parent != this) && !is_ancestor_of(new_parent) && !destroying &&
        !new_parent->destroying && (entry->region() == this),
      Failure::invalid_region_state);

    parent = new_parent;
    entry_point = entry;

    if (stack_reference_count != 0)
      new_parent->stack_inc();
  }

  void Region::clear_parent()
  {
    internal_check(parent != nullptr, Failure::invalid_region_state);

    auto* previous_parent = parent;
    parent = nullptr;
    entry_point = nullptr;

    if (stack_reference_count != 0)
      previous_parent->stack_dec();
    else
      destroy_region(this);
  }

  Region* frame_region(Frame* frame)
  {
    internal_check(frame != nullptr, Failure::invalid_region_state);

    if (frame->region != nullptr)
      return frame->region;

    frame->region =
      Region::create(RegionType::rc, frame->frame_id.stack_index() + 1);
    return frame->region;
  }

  Region* current_frame_region()
  {
    auto* context = ThreadContext::try_get();
    internal_check(
      (context != nullptr) && (context->thread.frame != nullptr),
      Failure::invalid_region_state);

    return frame_region(context->thread.frame);
  }

  void destroy_region(Region* region)
  {
    if ((region == nullptr) || region->destroying)
      return;

    internal_check(region->parent == nullptr, Failure::invalid_region_state);

    region->destroying = true;
    const bool began_finalizing = region->begin_finalizing();
    internal_check(began_finalizing, Failure::invalid_region_state);

    region->finalize_contents();
    region->release_dead_objects();
  }

  void destroy_frame_region(Frame* frame)
  {
    internal_check(
      (frame != nullptr) && (frame->region != nullptr),
      Failure::invalid_region_state);

    auto* region = frame->region;
    frame->region = nullptr;
    destroy_region(region);
  }
}
