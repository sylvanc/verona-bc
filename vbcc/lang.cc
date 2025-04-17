#include "lang.h"

#include <CLI/CLI.hpp>

namespace vbcc
{
  Node err(Node node, const std::string& msg)
  {
    return Error << (ErrorMsg ^ msg) << node;
  }

  std::optional<uint8_t> val(Node ptype)
  {
    if (ptype == Type)
      ptype = ptype / Type;

    if (ptype == None)
      return +ValueType::None;
    if (ptype == Bool)
      return +ValueType::Bool;
    if (ptype == I8)
      return +ValueType::I8;
    if (ptype == I16)
      return +ValueType::I16;
    if (ptype == I32)
      return +ValueType::I32;
    if (ptype == I64)
      return +ValueType::I64;
    if (ptype == U8)
      return +ValueType::U8;
    if (ptype == U16)
      return +ValueType::U16;
    if (ptype == U32)
      return +ValueType::U32;
    if (ptype == U64)
      return +ValueType::U64;
    if (ptype == F32)
      return +ValueType::F32;
    if (ptype == F64)
      return +ValueType::F64;

    return {};
  }

  Options& options()
  {
    static Options opts;
    return opts;
  }

  void Options::configure(CLI::App& cli)
  {
    cli.add_option(
      "-b,--bytecode", bytecode_file, "Output bytecode to this file.");
  }
}
