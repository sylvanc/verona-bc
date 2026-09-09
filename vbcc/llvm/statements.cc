#include "codegen.h"

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_statement(const Node& statement)
    {
      if ((statement == Source) || (statement == Offset))
        return true;

      if (statement == Const)
        return emit_const(statement);

      if (statement == Convert)
        return emit_convert(statement);

      if (statement->type().in({Add, Sub, Mul, Div, Mod, Pow,     And,
                                Or,  Xor, Shl, Shr, Eq,  Ne,      Lt,
                                Le,  Gt,  Ge,  Min, Max, LogBase, Atan2}))
        return emit_binop(statement);

      if (statement->type().in({Neg,  Not,     Abs,   Ceil,  Floor, Exp,
                                Log,  Sqrt,    Cbrt,  IsInf, IsNaN, Sin,
                                Cos,  Tan,     Asin,  Acos,  Atan,  Sinh,
                                Cosh, Tanh,    Asinh, Acosh, Atanh, Bits,
                                Len,  MakePtr, Read}))
        return emit_unop(statement);

      if (statement == Copy)
        return emit_copy(statement);

      if (statement == Move)
        return emit_move(statement);

      if (statement == GetRaise)
        return emit_get_raise(statement);

      if (statement == SetRaise)
        return emit_set_raise(statement);

      if (statement == Call)
        return emit_call(statement);

      if (statement == FFI)
        return emit_ffi(statement);

      if (statement == Drop)
        return emit_drop(statement);

      fail(
        statement,
        "unsupported statement '" + std::string(statement->type().str()) + "'");
      return false;
    }
  }
}
