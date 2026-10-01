#pragma once

#include "value.h"

namespace vrt
{
  /** Merge two mutable values into one RC region or raise a public error. */
  void merge(Value left, Value right);
}
