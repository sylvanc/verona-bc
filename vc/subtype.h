#pragma once

#include "bounds.h"
#include "lang.h"

namespace vc
{
  bool subtype(const Node& l, const Node& r);
  bool subtype(const Node& l, const Node& r, BoundsMap& bounds);
}
