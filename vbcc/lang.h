#pragma once

#include "bytecode.h"
#include "from_chars.h"

#include <vbcc.h>

namespace vbcc
{
  using namespace trieste;

  // Symbols.
  inline const auto Equals = TokenDef("=");
  inline const auto LParen = TokenDef("lparen");
  inline const auto RParen = TokenDef("rparen");
  inline const auto LBracket = TokenDef("lbracket");
  inline const auto RBracket = TokenDef("rbracket");
  inline const auto Comma = TokenDef(",");
  inline const auto Colon = TokenDef(":");

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
      Atanh,
      Bits,
      Len,
      MakePtr,
      Read);

  const auto Constant = T(Const_E, Const_Pi, Const_Inf, Const_NaN);

  const auto Def = Unop / Binop / Constant /
    T(Global,
      Const,
      ConstStr,
      Convert,
      New,
      Stack,
      Heap,
      Region,
      NewArray,
      NewArrayConst,
      StackArray,
      StackArrayConst,
      HeapArray,
      HeapArrayConst,
      RegionArray,
      RegionArrayConst,
      Copy,
      Move,
      RegisterRef,
      FieldRef,
      ArrayRef,
      ArrayRefConst,
      Load,
      Store,
      Lookup,
      FnPointer,
      Call,
      CallDyn,
      Subcall,
      SubcallDyn,
      Try,
      TryDyn,
      FFI,
      When,
      WhenDyn,
      Typetest);

  Parse parser();
  PassDef statements();
  PassDef labels();
  PassDef assignids(std::shared_ptr<Bytecode> state);
  PassDef validids(std::shared_ptr<Bytecode> state);
  PassDef liveness(std::shared_ptr<Bytecode> state);

  Node err(const std::string& msg);
  Node err(Node node, const std::string& msg);
  Node errmsg(const std::string& msg);
  Node errloc(Node node);
  ValueType val(Node ptype);
  std::string unescape(const std::string_view& in);

  template<typename T, char Sep = '_'>
  std::from_chars_result from_chars_sep(Node& node, T& t)
  {
    auto sv = node->location().view();

    // Fast path if no underscores.
    if (sv.find(Sep) == std::string_view::npos)
    {
      auto first = sv.data();
      auto last = first + sv.size();

      if constexpr (std::is_integral_v<T>)
      {
        if (node == Bin)
          return std::from_chars(first + 2, last, t, 2);
        if (node == Oct)
          return std::from_chars(first + 2, last, t, 8);
        if (node == Hex)
          return std::from_chars(first + 2, last, t, 16);
        if (node == Int)
          return std::from_chars(first, last, t, 10);
      }
      else if constexpr (std::is_floating_point_v<T>)
      {
        if (node->in({Float, HexFloat}))
          return std::from_chars(first, last, t);
      }

      return {first, std::errc::invalid_argument};
    }

    // Copy, stripping underscores.
    std::string stripped;
    stripped.reserve(sv.size());

    for (char c : sv)
    {
      if (c != '_')
        stripped.push_back(c);
    }

    auto first = stripped.data();
    auto last = first + stripped.size();

    if constexpr (std::is_integral_v<T>)
    {
      if (node == Bin)
        return std::from_chars(first + 2, last, t, 2);
      if (node == Oct)
        return std::from_chars(first + 2, last, t, 8);
      if (node == Hex)
        return std::from_chars(first + 2, last, t, 16);
      if (node == Int)
        return std::from_chars(first, last, t, 10);
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
      if (node->in({Float, HexFloat}))
        return std::from_chars(first, last, t);
    }

    return {first, std::errc::invalid_argument};
  }
}
