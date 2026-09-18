#pragma once

#include <vrt/object.h>

namespace vrt
{
  struct Header;
  struct Object;
  struct Region;
}

namespace vrt::writebarrier
{
  /** Consume one payload-shaped argument into a newly allocated field. */
  void init(
    Region* store_region, void* target, const Field& field, const void* source);

  /** Copy one encoded value over an existing field. */
  void copy(
    Region* store_region, void* target, const Field& field, const void* source);

  /** Drop a field while finalizing its containing object. */
  void drop(Region* store_region, const Field& field, void* source);

  /** Drag a frame-local object graph to an older or non-frame region. */
  bool
  drag(Region* destination, Header* root, bool root_reference_is_move = true);
}
