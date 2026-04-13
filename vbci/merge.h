#pragma once

#include "register.h"

namespace vbci
{
  // Merge two values into the same mutable region.
  void merge(const Register& a, const Register& b);
}
