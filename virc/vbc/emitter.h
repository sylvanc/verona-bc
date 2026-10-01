#pragma once

#include "../model/compilation.h"

namespace virc::vbc_backend
{
  void emit(
    const Compilation& bytecode,
    const std::filesystem::path& output,
    bool strip);
}
