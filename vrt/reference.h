#pragma once

#include <vrt/reference.h>

namespace vrt
{
  struct Frame;
}

namespace vrt::reference
{
  void escape_to(const Reference& reference, Frame* target);
}
