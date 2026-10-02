#pragma once

#include "../model/compilation.h"

namespace virc::llvm
{
  bool
  emit(const Compilation& compilation, const std::filesystem::path& output);
}
