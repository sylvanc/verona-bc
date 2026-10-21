#include "drag.h"

#include "failure.h"
#include "header.h"
#include "region.h"
#include "region_ext.h"

#include <limits>
#include <unordered_map>
#include <vector>

namespace vrt
{
  std::optional<DragResult>
  drag_allocation(Region* destination, Header* root, DragOptions options)
  {
    if (
      (destination == nullptr) || destination->destroying ||
      destination->is_finalizing() || (root == nullptr) ||
      root->location().is_immortal() || root->finalizing ||
      (root->region() == nullptr) || !root->region()->is_frame_local())
      return {};

    if (root->region() == destination)
      return DragResult{};

    DragResult result;
    std::vector<Header*> worklist;
    std::unordered_map<Header*, uintptr_t> internal_references;
    std::unordered_map<Region*, Header*> child_regions;
    uintptr_t destination_stack_decrements = 0;
    worklist.push_back(root);

    while (!worklist.empty())
    {
      auto* header = worklist.back();
      worklist.pop_back();

      auto existing = internal_references.find(header);
      if (existing != internal_references.end())
      {
        if (existing->second == std::numeric_limits<uintptr_t>::max())
          return {};

        existing->second++;
        continue;
      }

      const auto location = header->location();
      if (location.is_immutable() || location.is_immortal())
        continue;

      auto* source_region = header->region();
      if (
        (source_region == nullptr) || source_region->destroying ||
        header->finalizing)
        return {};

      if (source_region == destination)
      {
        if (!destination->is_frame_local())
        {
          if (
            destination_stack_decrements ==
            std::numeric_limits<uintptr_t>::max())
            return {};

          destination_stack_decrements++;
        }

        continue;
      }

      if (
        destination->is_frame_local() && source_region->is_frame_local() &&
        (destination->frame_depth >= source_region->frame_depth))
        continue;

      if (!source_region->is_frame_local())
      {
        if (destination->is_frame_local())
          continue;

        if (
          source_region->has_parent() &&
          (source_region->parent == destination))
        {
          if (
            (source_region != options.replaced_child) ||
            result.replaced_child_reused)
            return {};

          result.replaced_child_reused = true;
          continue;
        }

        if (
          source_region->has_parent() ||
          source_region->is_ancestor_of(destination) ||
          !child_regions.emplace(source_region, header).second)
          return {};

        continue;
      }

      internal_references.emplace(header, 1);
      header->trace_fn([&](Header* child) { worklist.push_back(child); });
    }

    if (options.root_reference == RootReference::retained)
    {
      auto root_references = internal_references.find(root);
      if (
        (root_references == internal_references.end()) ||
        (root_references->second == 0))
        return {};

      root_references->second--;
    }

    for (const auto& [header, internal_count] : internal_references)
    {
      if (
        (header->reference_count < internal_count) ||
        (header->region() == nullptr) || !header->region()->contains(header))
        return {};
    }

    if (
      result.replaced_child_reused &&
      (options.replaced_child->stack_reference_count == 0))
      return {};

    destination->stack_inc();

    for (const auto& [region, entry] : child_regions)
    {
      region->set_parent(destination, entry);
      const bool child_region_alive = region->stack_dec();
      internal_check(child_region_alive, Failure::invalid_write);
    }

    if (result.replaced_child_reused)
    {
      const bool child_region_alive = options.replaced_child->stack_dec();
      internal_check(child_region_alive, Failure::invalid_write);
    }

    for (const auto& [header, internal_count] : internal_references)
    {
      auto* source_region = header->region();
      const auto external_count = header->reference_count - internal_count;
      destination->stack_inc(external_count);

      const bool removed = source_region->remove(header);
      internal_check(removed, Failure::invalid_write);

      header->set_location(Location(destination));
      destination->insert(header);
    }

    if (destination_stack_decrements != 0)
    {
      const bool destination_alive =
        destination->stack_dec(destination_stack_decrements);
      internal_check(destination_alive, Failure::invalid_write);
    }

    const bool destination_alive = destination->stack_dec();
    internal_check(destination_alive, Failure::invalid_write);

    return result;
  }
}