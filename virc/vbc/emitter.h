#pragma once

#include "../model/bytecode.h"

namespace virc::vbc_backend
{
  void emit(
    const Bytecode& bytecode,
    const std::filesystem::path& output,
    bool strip);
}
