#pragma once

#include "../model/compilation.h"
#include "encoder.h"

namespace virc::vbc_backend
{
  using MemoSlots = std::unordered_map<std::string, size_t>;

  void encode_statement(
    Compilation& compilation,
    FuncState& function,
    const MemoSlots& memo_slots,
    ByteBuffer& output,
    trieste::Node statement);

  void encode_terminator(
    Compilation& compilation,
    FuncState& function,
    ByteBuffer& output,
    trieste::Node terminator);
}
