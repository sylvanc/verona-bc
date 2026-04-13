#pragma once

#include "ident.h"

namespace vbci
{
  struct Region;
  struct Header;

  template<bool is_move>
  bool drag_allocation(Region* dest, Header* h, Region** pr = nullptr);
}
