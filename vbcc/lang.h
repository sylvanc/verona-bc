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
    T(Const,
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
      TryCallDyn,
      FFI,
      When,
      WhenDyn,
      GetRaise,
      SetRaise,
      Typetest,
      MakeCallback,
      CallbackPtr,
      FreeCallback,
      AddExternal,
      RemoveExternal);

  Parse parser();
  PassDef statements();
  PassDef labels();
  PassDef assignids(std::shared_ptr<Bytecode> state);
  PassDef validids(std::shared_ptr<Bytecode> state);
  PassDef liveness(std::shared_ptr<Bytecode> state);
  PassDef typecheck(std::shared_ptr<Bytecode> state);

  Node err(const std::string& msg);
  Node err(Node node, const std::string& msg);
  Node errmsg(const std::string& msg);
  Node errloc(Node node);
  ValueType val(Node ptype);
  std::string unescape(const std::string_view& in);

  template<typename T>
  std::from_chars_result
  from_chars_sep(const Node& node, const char* s, size_t n, T& t)
  {
    auto end = s + n;

    if constexpr (std::is_integral_v<T>)
    {
      if (node == Bin)
        return std::from_chars(s + 2, end, t, 2);
      if (node == Oct)
        return std::from_chars(s + 2, end, t, 8);
      if (node == Hex)
        return std::from_chars(s + 2, end, t, 16);
      if (node == Int)
        return std::from_chars(s, end, t, 10);
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
      if (node->in({Float, HexFloat}))
        return std::from_chars(s, end, t);
    }

    return {s, std::errc::invalid_argument};
  }

  template<typename T, char Sep = '_'>
  std::from_chars_result from_chars_sep(const Node& node, T& t)
  {
    auto sv = node->location().view();

    // Character literals.
    if (node == Char)
    {
      auto s = unescape(sv);

      if (s.starts_with("error:"))
        return {sv.data(), std::errc::invalid_argument};

      if (s.empty() || (s.size() > sizeof(uint64_t)))
        return {sv.data(), std::errc::result_out_of_range};

      // Pack bytes big-endian, like C/C++ multicharacter literals.
      uint64_t v = 0;

      for (auto c : s)
        v = (v << 8) | static_cast<uint8_t>(c);

      if constexpr (std::is_signed_v<T>)
      {
        if (v > static_cast<uint64_t>(std::numeric_limits<T>::max()))
          return {sv.data(), std::errc::result_out_of_range};
      }
      else
      {
        if (v > std::numeric_limits<T>::max())
          return {sv.data(), std::errc::result_out_of_range};
      }

      t = static_cast<T>(v);
      return {sv.data() + sv.size(), {}};
    }

    // Fast path if no underscores.
    if (sv.find(Sep) == std::string_view::npos)
      return from_chars_sep<T>(node, sv.data(), sv.size(), t);

    // Copy, stripping underscores.
    std::string stripped;
    stripped.reserve(sv.size());

    for (char c : sv)
    {
      if (c != '_')
        stripped.push_back(c);
    }

    return from_chars_sep<T>(node, stripped.data(), stripped.size(), t);
  }

  template<typename T, char Sep = '_'>
  T from_chars_sep_v(const Node& node)
  {
    T t = 0;
    from_chars_sep<T, Sep>(node, t);
    return t;
  }
}
