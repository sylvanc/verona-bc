#include "merge.h"

#include "drag.h"
#include "error.h"
#include "failure.h"
#include "header.h"
#include "region.h"
#include "region_ext.h"

#include <limits>
#include <optional>
#include <unordered_set>
#include <vector>

namespace vrt
{
  namespace
  {
    struct MergeOperand
    {
      Header* header;
      Region* region;
      bool stack;
    };

    struct RegionMergePlan
    {
      Region* destination;
      Region* source;
      std::vector<Header*> headers;
      std::vector<Region*> child_regions;
    };

    MergeOperand inspect(Value value)
    {
      auto* header = value.header();
      const auto location = header->location();
      if (location.is_pending())
        raise_error(Error::bad_merge);

      return {
        header,
        location.is_region() ? location.to_region() : nullptr,
        location.is_stack()};
    }

    std::optional<RegionMergePlan>
    plan_region_merge(Region* destination, Region* source)
    {
      if (
        (destination == nullptr) || (source == nullptr) ||
        (destination == source) || destination->is_frame_local() ||
        source->is_frame_local() || destination->is_arena() ||
        source->is_arena() || destination->destroying || source->destroying ||
        destination->is_finalizing() || source->is_finalizing() ||
        source->has_parent() || source->is_ancestor_of(destination))
        return {};

      RegionMergePlan plan{destination, source, {}, {}};
      source->for_each_header([&](Header* header) {
        plan.headers.push_back(header);
      });

      std::unordered_set<Region*> children;
      for (auto* header : plan.headers)
      {
        if (
          header->finalizing || (header->region() != source) ||
          !source->contains(header))
          return {};

        header->trace_fn([&](Header* child) {
          const auto location = child->location();
          if (!location.is_region())
            return;

          auto* child_region = location.to_region();
          if (
            (child_region != source) && (child_region != destination) &&
            child_region->has_parent() && (child_region->parent == source))
            children.emplace(child_region);
        });
      }

      const auto max = std::numeric_limits<RC>::max();
      if (
        (destination->stack_reference_count == max) ||
        (source->stack_reference_count == max) ||
        (source->stack_reference_count >
         (max - destination->stack_reference_count - 1)))
        return {};

      for (auto* child : children)
      {
        if (
          child->destroying || child->is_finalizing() ||
          (child->entry_point == nullptr) ||
          (child->entry_point->region() != child) ||
          child->is_ancestor_of(destination) ||
          (child->stack_reference_count == max))
          return {};

        if (
          (child->stack_reference_count == 0) &&
          ((source->stack_reference_count > (max - 2)) ||
           (destination->stack_reference_count > (max - 2))))
          return {};

        plan.child_regions.push_back(child);
      }

      return plan;
    }

    void commit_region_merge(const RegionMergePlan& plan)
    {
      auto* destination = plan.destination;
      auto* source = plan.source;
      destination->stack_inc();
      source->stack_inc();

      for (auto* header : plan.headers)
      {
        const bool removed = source->remove(header);
        internal_check(removed, Failure::invalid_region_state);
        header->set_location(Location(destination));
        destination->insert(header);
      }

      for (auto* child : plan.child_regions)
      {
        auto* entry_point = child->entry_point;
        child->stack_inc();
        child->clear_parent();
        child->set_parent(destination, entry_point);
        const bool child_alive = child->stack_dec();
        internal_check(child_alive, Failure::invalid_region_state);
      }

      const auto source_references = source->stack_reference_count - 1;
      destination->stack_inc(source_references);
      const bool source_guarded = source->stack_dec(source_references);
      internal_check(source_guarded, Failure::invalid_region_state);

      const bool source_released = source->stack_dec();
      internal_check(!source_released, Failure::invalid_region_state);
      destination->stack_dec();
    }

    [[noreturn]] void bad_merge()
    {
      raise_error(Error::bad_merge);
    }
  }

  void merge(Value left, Value right)
  {
    auto left_operand = inspect(left);
    auto right_operand = inspect(right);

    if (
      (left_operand.region == nullptr) && (right_operand.region == nullptr))
      return;

    if ((left_operand.region == nullptr) || (right_operand.region == nullptr))
    {
      if (left_operand.stack || right_operand.stack)
        raise_error(Error::bad_stack_escape);

      return;
    }

    if (left_operand.region == right_operand.region)
      return;

    const bool left_frame_local = left_operand.region->is_frame_local();
    const bool right_frame_local = right_operand.region->is_frame_local();
    if (left_frame_local && right_frame_local)
      return;

    if (left_frame_local != right_frame_local)
    {
      auto* destination =
        left_frame_local ? right_operand.region : left_operand.region;
      auto* source = left_frame_local ? left_operand.header : right_operand.header;
      if (
        destination->is_arena() || destination->destroying ||
        destination->is_finalizing() ||
        !drag_allocation(
          destination,
          source,
          {.root_reference = RootReference::retained}))
        bad_merge();

      return;
    }

    const bool left_owned = left_operand.region->has_parent();
    const bool right_owned = right_operand.region->has_parent();
    if (left_owned && right_owned)
      bad_merge();

    auto* destination =
      right_owned ? right_operand.region : left_operand.region;
    auto* source = right_owned ? left_operand.region : right_operand.region;
    auto plan = plan_region_merge(destination, source);
    if (!plan)
      bad_merge();

    commit_region_merge(*plan);
  }
}
