#include "merge.h"

#include "array.h"
#include "drag.h"
#include "header.h"
#include "object.h"
#include "program.h"
#include "region_ext.h"

#include <unordered_map>
#include <vector>

namespace vbci
{
  // Helper: get the region for a mergeable value, or nullptr if not applicable.
  static Region* merge_region(const Register& v)
  {
    if (v->is_readonly())
      return nullptr;

    if (!v->is_header())
      return nullptr;

    auto loc = v->location();

    if (loc.is_immutable() || loc.is_immortal() || loc.is_stack())
      return nullptr;

    if (loc.is_region())
      return loc.to_region();

    return nullptr;
  }

  static bool merge_is_stack(const Register& v)
  {
    if (v->is_readonly())
      return false;

    if (!v->is_header())
      return false;

    return v->location().is_stack();
  }

  static void merge_regions(Region* dest, Region* src)
  {
    auto& program = Program::get();

    // Guard dest against premature free.
    dest->stack_inc();

    // Collect all headers from src.
    std::vector<Header*> to_move;
    src->for_each_header([&](Header* h) { to_move.push_back(h); });

    // Find sub-regions parented to src by scanning src objects' fields.
    std::unordered_map<Region*, Header*> sub_regions;
    auto scan_fn = [&](Header* child) {
      auto child_loc = child->location();

      if (!child_loc.is_region())
        return;

      auto child_r = child_loc.to_region();

      if (child_r == src || child_r == dest)
        return;

      if (child_r->has_parent() && child_r->get_parent() == src)
        sub_regions.emplace(child_r, child);
    };

    for (auto* h : to_move)
    {
      if (program.is_array(h->get_type_id()))
        static_cast<Array*>(h)->trace_fn(scan_fn);
      else
        static_cast<Object*>(h)->trace_fn(scan_fn);
    }

    // Move all objects from src to dest.
    for (auto* h : to_move)
      h->move_region(dest);

    // Reparent sub-regions from src to dest.
    for (auto& [sub_r, entry] : sub_regions)
    {
      // Protect the sub-region against premature free.
      sub_r->stack_inc();

      // clear_parent cascades to src, but src is guarded.
      sub_r->clear_parent();
      sub_r->set_parent(dest, entry);

      // Undo protection.
      sub_r->stack_dec();
    }

    // Transfer src's remaining stack_rc to dest.
    auto src_stack_rc = src->get_stack_rc();

    if (src_stack_rc > 0)
      dest->stack_inc(src_stack_rc);

    // Remove dest guard.
    dest->stack_dec();

    // Dispose of the empty src region.
    if (src_stack_rc > 0)
      src->stack_dec(src_stack_rc);
    else
      src->free_region();
  }

  void merge(const Register& a, const Register& b)
  {
    bool a_stack = merge_is_stack(a);
    bool b_stack = merge_is_stack(b);
    Region* a_r = merge_region(a);
    Region* b_r = merge_region(b);

    // Both nullptr — noop (primitives, cowns, readonly, immutable, immortal).
    if (!a_r && !b_r)
    {
      if (a_stack && b_r)
        Value::error(Error::BadStackEscape);
      if (b_stack && a_r)
        Value::error(Error::BadStackEscape);
      return;
    }

    // One or both have no region — check stack escapes.
    if (!a_r)
    {
      if (a_stack)
        Value::error(Error::BadStackEscape);
      return;
    }

    if (!b_r)
    {
      if (b_stack)
        Value::error(Error::BadStackEscape);
      return;
    }

    // Same region — noop.
    if (a_r == b_r)
      return;

    bool a_fl = a_r->is_frame_local();
    bool b_fl = b_r->is_frame_local();

    // Both frame-local — noop.
    if (a_fl && b_fl)
      return;

    // a=heap, b=frame-local → drag b into a's region.
    if (!a_fl && b_fl)
    {
      if (!drag_allocation<false>(a_r, b->get_header()))
        Value::error(Error::BadMerge);
      return;
    }

    // a=frame-local, b=heap → drag a into b's region.
    if (a_fl && !b_fl)
    {
      if (!drag_allocation<false>(b_r, a->get_header()))
        Value::error(Error::BadMerge);
      return;
    }

    // Both heap regions — combine.
    bool a_owned = a_r->has_owner();
    bool b_owned = b_r->has_owner();

    if (a_owned && b_owned)
      Value::error(Error::BadMerge);

    if (b_owned)
      merge_regions(b_r, a_r);
    else
      merge_regions(a_r, b_r);
  }
}
