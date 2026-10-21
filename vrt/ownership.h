#pragma once

#include "value.h"

namespace vrt::ownership
{
  void retain_root(Value value);
  void release_root(Value value);
  void pin(Value value);
  void unpin(Value value);
  void retain_field(Value value);
  void release_field(Value value);
  void escape(Value value);
  void validate_tailcall(Value value);
}
