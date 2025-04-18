#pragma once

#include "bytecode.h"
#include "lang.h"

#include <vbcc.h>

namespace vbcc
{
  using namespace trieste;

  const auto IntType = T(I8, I16, I32, I64, U8, U16, U32, U64);
  const auto FloatType = T(F32, F64);
  const auto PrimitiveType = T(None, Bool) / IntType / FloatType;
  const auto BaseType = PrimitiveType / T(Dyn);

  const auto IntLiteral = T(Bin, Oct, Hex, Int);
  const auto FloatLiteral = T(Float, HexFloat);

  const auto Binop =
    T(Add,
      Sub,
      Mul,
      Div,
      Mod,
      Pow,
      And,
      Or,
      Xor,
      Shl,
      Shr,
      Eq,
      Ne,
      Lt,
      Le,
      Gt,
      Ge,
      Min,
      Max,
      LogBase,
      Atan2);

  const auto Unop =
    T(Neg,
      Not,
      Abs,
      Ceil,
      Floor,
      Exp,
      Log,
      Sqrt,
      Cbrt,
      IsInf,
      IsNaN,
      Sin,
      Cos,
      Tan,
      Asin,
      Acos,
      Atan,
      Sinh,
      Cosh,
      Tanh,
      Asinh,
      Acosh,
      Atanh);

  const auto Constant = T(Const_E, Const_Pi, Const_Inf, Const_NaN);

  const auto Def = Unop / Binop / Constant /
    T(Global,
      Const,
      Convert,
      Stack,
      Heap,
      Region,
      StackArray,
      StackArrayConst,
      HeapArray,
      HeapArrayConst,
      RegionArray,
      RegionArrayConst,
      Copy,
      Move,
      Ref,
      ArrayRef,
      ArrayRefConst,
      Load,
      Store,
      Lookup,
      FnPointer,
      Call,
      CallDyn);

  const auto Statement = Def / T(Drop, Arg);
  const auto Terminator =
    T(Tailcall, TailcallDyn, Return, Raise, Throw, Cond, Jump);

  Parse parser();
  PassDef statements();
  PassDef labels();
  PassDef assignids(std::shared_ptr<State> state);
  PassDef validids(std::shared_ptr<State> state);

  struct Options : public trieste::Options
  {
    std::string bytecode_file;
    void configure(CLI::App& cli) override;
  };

  Node err(Node node, const std::string& msg);
  std::optional<uint8_t> val(Node ptype);
  Options& options();
}
