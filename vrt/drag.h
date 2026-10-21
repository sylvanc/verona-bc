#pragma once

#include <optional>

namespace vrt
{
  struct Header;
  struct Region;

  enum class RootReference
  {
    transferred,
    retained
  };

  struct DragOptions
  {
    RootReference root_reference = RootReference::transferred;
    Region* replaced_child = nullptr;
  };

  struct DragResult
  {
    bool replaced_child_reused = false;
  };

  /** Relocate a frame-local graph into an older or non-frame region. */
  [[nodiscard]] std::optional<DragResult> drag_allocation(
    Region* destination,
    Header* root,
    DragOptions options = {});
}