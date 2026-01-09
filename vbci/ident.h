#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <string>

namespace vbci
{
  enum class Error
  {
    UnknownGlobal,
    UnknownFFI,
    UnknownRegionType,
    UnknownOpcode,
    TooManyRefs,
    BadAllocTarget,
    BadLabel,
    BadField,
    BadArrayIndex,
    BadRefTarget,
    BadLoadTarget,
    BadStoreTarget,
    BadStore,
    BadConversion,
    BadOperand,
    MismatchedTypes,
    MethodNotFound,
    BadStackEscape,
    BadArgs,
    BadType,
    BadRegionEntryPoint,
  };

  using PC = size_t;
  struct Location;
  using RC = uint32_t;
  using ARC = std::atomic<RC>;

  struct Region;
  struct Header;
  struct Object;
  struct Array;
  struct Cown;
  struct Function;
  struct Program;

  inline std::string errormsg(Error error)
  {
    switch (error)
    {
      case Error::UnknownGlobal:
        return "unknown global";
      case Error::UnknownFFI:
        return "unknown FFI";
      case Error::UnknownRegionType:
        return "unknown region type";
      case Error::UnknownOpcode:
        return "unknown opcode";
      case Error::BadAllocTarget:
        return "bad alloc target";
      case Error::BadLabel:
        return "bad label";
      case Error::BadField:
        return "bad field";
      case Error::BadArrayIndex:
        return "bad array index";
      case Error::BadRefTarget:
        return "bad ref target";
      case Error::BadLoadTarget:
        return "bad load target";
      case Error::BadStoreTarget:
        return "bad store target";
      case Error::BadStore:
        return "bad store";
      case Error::BadConversion:
        return "bad conversion";
      case Error::BadOperand:
        return "bad operand";
      case Error::MismatchedTypes:
        return "mismatched types";
      case Error::MethodNotFound:
        return "method not found";
      case Error::BadStackEscape:
        return "bad stack escape";
      case Error::BadArgs:
        return "bad args";
      case Error::BadType:
        return "bad type";
      case Error::BadRegionEntryPoint:
        return "bad region entry point";

      default:
        assert(false);
        return "unknown error";
    }
  }
}
