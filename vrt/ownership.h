#pragma once

#include "value.h"

namespace vrt::ownership
{
  void retain_root(Value value);
  void release_root(Value value);
  void retain_field(Value value);
  void release_field(Value value);
  void escape(Value value);
}
