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
    BadAllocTarget,
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
    BadStackEscape,
  };

  using PC = size_t;
  using Local = uint8_t;
  using FieldIdx = uint32_t;
  using Location = uintptr_t;
  using RC = uint32_t;
  using ARC = std::atomic<RC>;

  struct Region;
  struct Object;
  struct Array;
  struct Cown;
  struct Function;
  struct Program;
}
