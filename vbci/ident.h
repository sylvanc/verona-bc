#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>

namespace vbci
{
  enum class Error : uint8_t
  {
    UnknownGlobal,
    UnknownFunction,
    UnknownPrimitiveType,
    UnknownArgType,
    UnknownCallType,
    UnknownOpcode,
    UnknownMathOp,
    StackOutOfBounds,
    BadLabel,
    BadField,
    BadRefTarget,
    BadLoadTarget,
    BadStoreTarget,
    BadStore,
    BadMethodTarget,
    BadConditional,
    BadConversion,
    BadOperand,
    MismatchedTypes,
    MethodNotFound,
    BadReturnLocation,
  };

  using PC = size_t;
  using Local = uint8_t;
  using FieldIdx = uint16_t;
  using Location = uintptr_t;
  using RC = uint32_t;
  using ARC = std::atomic<RC>;

  constexpr size_t registers = 256;

  struct Object;
  struct Cown;
  struct Function;
  struct Program;
}
