#pragma once

#include "../model/compilation.h"
#include "encoder.h"

namespace virc::vbc_backend
{
  using MemoSlots = std::unordered_map<std::string, size_t>;

  void encode_statement(
    Compilation& state,
    FuncState& func_state,
    const MemoSlots& memo_slot_map,
    ByteBuffer& code,
    trieste::Node stmt);

  void encode_terminator(
    Compilation& state,
    FuncState& func_state,
    ByteBuffer& code,
    trieste::Node term);
  }