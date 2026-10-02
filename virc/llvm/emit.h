#pragma once

#include "../model/compilation.h"

namespace virc::llvm_backend
{
  bool emit(
    const Compilation& compilation, const std::filesystem::path& output);
}

namespace virc::llvm
{
  bool emit(
    const Compilation& compilation, const std::filesystem::path& output);
}
