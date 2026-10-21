#pragma once

#include "../model/compilation.h"

namespace virc::vbc
{
  void emit(
    const Compilation& compilation,
    const std::filesystem::path& output,
    bool strip);
}