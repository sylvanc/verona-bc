#include "codegen.h"

namespace virc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::emit_statement(const Node& statement)
    {
      if ((statement == vir::Source) || (statement == Offset))
        return true;

      if (statement == Const)
        return emit_const(statement);

      if (statement == Const_E)
        return emit_const_e(statement);

      if (statement == Const_Pi)
        return emit_const_pi(statement);

      if (statement == Const_Inf)
        return emit_const_inf(statement);

      if (statement == Const_NaN)
        return emit_const_nan(statement);

      if (statement == AddExternal)
        return emit_add_external(statement);

      if (statement == RemoveExternal)
        return emit_remove_external(statement);

      if (statement == Convert)
        return emit_convert(statement);

      if (statement->type().in({Add, Sub, Mul, Div, Mod, Pow,     And,
                                Or,  Xor, Shl, Shr, Eq,  Ne,      Lt,
                                Le,  Gt,  Ge,  Min, Max, LogBase, Atan2}))
        return emit_binop(statement);

      if (statement->type().in(
            {Neg,   Not,   Abs,   Ceil,  Floor, Exp,  Log,  Sqrt,    Cbrt,
             IsInf, IsNaN, Sin,   Cos,   Tan,   Asin, Acos, Atan,    Sinh,
             Cosh,  Tanh,  Asinh, Acosh, Atanh, Bits, Len,  MakePtr, Read}))
        return emit_unop(statement);

      if (statement == Copy)
        return emit_copy(statement);

      if (statement == Move)
        return emit_move(statement);

      if (statement == Freeze)
        return emit_freeze(statement);

      if (statement->type().in({Pin, Unpin}))
        return emit_pin(statement);

      if (statement == Merge)
        return emit_merge(statement);

      if (statement == GetRaise)
        return emit_get_raise(statement);

      if (statement == SetRaise)
        return emit_set_raise(statement);

      if (statement == Call)
        return emit_call(statement);

      if (statement == Lookup)
        return emit_lookup(statement);

      if (statement == CallDyn)
        return emit_call_dyn(statement);

      if (statement == FFI)
        return emit_ffi(statement);

      if (statement == Drop)
        return emit_drop(statement);

      if (statement == Singleton)
        return emit_singleton(statement);

      if (statement == New)
        return emit_new(statement);

      if (statement == Stack)
        return emit_stack(statement);

      if (statement == NewArray)
        return emit_new_array(statement);

      if (statement == NewArrayConst)
        return emit_new_array_const(statement);

      if (statement->type().in({StackArray, StackArrayConst}))
        return emit_stack_array(statement);

      if (statement->type().in({HeapArray, HeapArrayConst}))
        return emit_heap_array(statement);

      if (statement->type().in({RegionArray, RegionArrayConst}))
        return emit_region_array(statement);

      if (statement == RegisterRef)
        return emit_register_ref(statement);

      if (statement == FieldRef)
        return emit_field_ref(statement);

      if (statement == ArrayRef)
        return emit_array_ref(statement);

      if (statement == ArrayRefConst)
        return emit_array_ref_const(statement);

      if (statement == Load)
        return emit_load(statement);

      if (statement == Store)
        return emit_store(statement);

      if (statement == ArrayCopy)
        return emit_array_copy(statement);

      if (statement == ArrayFill)
        return emit_array_fill(statement);

      if (statement == ArrayCompare)
        return emit_array_compare(statement);

      if (statement == Heap)
        return emit_heap(statement);

      if (statement == Region)
        return emit_region(statement);

      fail(
        statement,
        "unsupported statement '" + std::string(statement->type().str()) + "'");
      return false;
    }
  }
}
