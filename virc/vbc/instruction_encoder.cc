#include "instruction_encoder.h"

#include "../lang.h"
#include "type_encoding.h"

#include <vbc/format.h>

namespace virc::vbc_backend
{
  using namespace trieste;
  using namespace vir;
  using namespace ::vbc;

  namespace
  {
    class InstructionEncoder
    {
    private:
      Compilation& state;
      FuncState& func_state;
      const MemoSlots& memo_slot_map;
      ByteBuffer& code;

      uleb<size_t> dst(Node stmt)
      {
        return uleb(*func_state.get_register_id(stmt / LocalId));
      }

      uleb<size_t> lhs(Node stmt)
      {
        return uleb(*func_state.get_register_id(stmt / Lhs));
      }

      uleb<size_t> rhs(Node stmt)
      {
        return uleb(*func_state.get_register_id(stmt / Rhs));
      }

      uleb<size_t> src(Node stmt)
      {
        return rhs(stmt);
      }

      uleb<size_t> cls(Node stmt)
      {
        return uleb(state.type_id(stmt / ClassId));
      }

      uleb<size_t> fld(Node stmt)
      {
        return uleb(*state.get_field_id(stmt / FieldId));
      }

      uleb<size_t> mth(Node stmt)
      {
        return uleb(*state.get_method_id(stmt / MethodId));
      }

      uleb<size_t> fn(Node stmt)
      {
        return uleb(*state.get_func_id(stmt / FunctionId));
      }

      void onearg(Node onearg)
      {
        if ((onearg / Type) == ArgMove)
          code << uleb(+Op::ArgMove) << uleb(src(onearg));
        else
          code << uleb(+Op::ArgCopy) << uleb(src(onearg));
      }

      void args(Node args)
      {
        for (auto argument_node : *args)
          onearg(argument_node);
      }

      void binary(Node stmt, Op op)
      {
        code << uleb(+op) << dst(stmt) << lhs(stmt) << rhs(stmt);
      }

      void unary(Node stmt, Op op)
      {
        code << uleb(+op) << dst(stmt) << src(stmt);
      }

    public:
      InstructionEncoder(
        Compilation& state,
        FuncState& func_state,
        const MemoSlots& memo_slot_map,
        ByteBuffer& code)
      : state(state),
        func_state(func_state),
        memo_slot_map(memo_slot_map),
        code(code)
      {}

      void stmt(Node stmt)
      {
        if (stmt == Const)
        {
          auto type = stmt / Type;
          auto value = stmt / Rhs;

          if (type == None)
          {
            code << uleb(+Op::Const) << dst(stmt) << uleb(+val(type));
          }
          else if (type == Bool)
          {
            code << uleb(+Op::Const) << dst(stmt) << uleb(+val(type));
            code << uleb(((stmt / Rhs) == True) ? 1 : 0);
          }
          else if (type == I8)
          {
            code << uleb(+Op::Const) << dst(stmt) << uleb(+val(type))
                   << sleb(from_chars_sep_v<int8_t>(value));
          }
          else if (type == U8)
          {
            code << uleb(+Op::Const) << dst(stmt) << uleb(+val(type))
                   << uleb(from_chars_sep_v<uint8_t>(value));
          }
          else if (type == I16)
          {
            code << uleb(+Op::Const) << dst(stmt) << uleb(+val(type))
                   << sleb(from_chars_sep_v<int16_t>(value));
          }
          else if (type == U16)
          {
            code << uleb(+Op::Const) << dst(stmt) << uleb(+val(type))
                   << uleb(from_chars_sep_v<uint16_t>(value));
          }
          else if (type == I32)
          {
            code << uleb(+Op::Const) << dst(stmt) << uleb(+val(type))
                   << sleb(from_chars_sep_v<int32_t>(value));
          }
          else if (type == U32)
          {
            code << uleb(+Op::Const) << dst(stmt) << uleb(+val(type))
                   << uleb(from_chars_sep_v<uint32_t>(value));
          }
          else if (type->in({I64, ILong, ISize}))
          {
            code << uleb(+Op::Const) << dst(stmt) << uleb(+val(type))
                   << sleb(from_chars_sep_v<int64_t>(value));
          }
          else if (type->in({U64, ULong, USize, Ptr}))
          {
            code << uleb(+Op::Const) << dst(stmt) << uleb(+val(type))
                   << uleb(from_chars_sep_v<uint64_t>(value));
          }
          else if (type == F32)
          {
            code << uleb(+Op::Const) << dst(stmt) << uleb(+val(type))
                   << sleb(from_chars_sep_v<float>(value));
          }
          else if (type == F64)
          {
            code << uleb(+Op::Const) << dst(stmt) << uleb(+val(type))
                   << sleb(from_chars_sep_v<double>(value));
          }
        }
        else if (stmt == ConstStr)
        {
          code << uleb(+Op::String) << dst(stmt)
                 << uleb(ST::exec().string(stmt / String));
        }
        else if (stmt == Convert)
        {
          code << uleb(+Op::Convert) << dst(stmt)
                 << uleb(+val(stmt / Type)) << rhs(stmt);
        }
        else if (stmt == Singleton)
        {
          code << uleb(+Op::Singleton) << dst(stmt) << cls(stmt);
        }
        else if (stmt == New)
        {
          args(stmt / Args);
          code << uleb(+Op::New) << dst(stmt) << cls(stmt);
        }
        else if (stmt == Stack)
        {
          args(stmt / Args);
          code << uleb(+Op::Stack) << dst(stmt) << cls(stmt);
        }
        else if (stmt == Heap)
        {
          args(stmt / Args);
          code << uleb(+Op::Heap) << dst(stmt) << rhs(stmt)
                 << cls(stmt);
        }
        else if (stmt == Region)
        {
          args(stmt / Args);
          code << uleb(+Op::Region) << dst(stmt)
                 << encode_region(stmt) << cls(stmt);
        }
        else if (stmt == NewArray)
        {
          code << uleb(+Op::NewArray) << dst(stmt) << rhs(stmt)
                 << uleb(state.type_id(stmt / Type));
        }
        else if (stmt == NewArrayConst)
        {
          code << uleb(+Op::NewArrayConst) << dst(stmt)
                 << uleb(state.type_id(stmt / Type))
                 << uleb(from_chars_sep_v<uint64_t>(stmt / Rhs));
        }
        else if (stmt == StackArray)
        {
          code << uleb(+Op::StackArray) << dst(stmt) << rhs(stmt)
                 << uleb(state.type_id(stmt / Type));
        }
        else if (stmt == StackArrayConst)
        {
          code << uleb(+Op::StackArrayConst) << dst(stmt)
                 << uleb(state.type_id(stmt / Type))
                 << uleb(from_chars_sep_v<uint64_t>(stmt / Rhs));
        }
        else if (stmt == HeapArray)
        {
          code << uleb(+Op::HeapArray) << dst(stmt) << lhs(stmt)
                 << rhs(stmt)
                 << uleb(state.type_id(stmt / Type));
        }
        else if (stmt == HeapArrayConst)
        {
          code << uleb(+Op::HeapArrayConst) << dst(stmt)
                 << lhs(stmt)
                 << uleb(state.type_id(stmt / Type))
                 << uleb(from_chars_sep_v<uint64_t>(stmt / Rhs));
        }
        else if (stmt == RegionArray)
        {
          code << uleb(+Op::RegionArray) << dst(stmt)
                 << encode_region(stmt) << rhs(stmt)
                 << uleb(state.type_id(stmt / Type));
        }
        else if (stmt == RegionArrayConst)
        {
          code << uleb(+Op::RegionArrayConst) << dst(stmt)
                 << encode_region(stmt)
                 << uleb(state.type_id(stmt / Type))
                 << uleb(from_chars_sep_v<uint64_t>(stmt / Rhs));
        }
        else if (stmt == Copy)
        {
          code << uleb(+Op::Copy) << dst(stmt) << src(stmt);
        }
        else if (stmt == Move)
        {
          code << uleb(+Op::Move) << dst(stmt) << src(stmt);
        }
        else if (stmt == Drop)
        {
          code << uleb(+Op::Drop) << dst(stmt);
        }
        else if (stmt == Freeze)
        {
          code << uleb(+Op::Freeze) << dst(stmt) << src(stmt);
        }
        else if (stmt == RegisterRef)
        {
          code << uleb(+Op::RegisterRef) << dst(stmt) << src(stmt);
        }
        else if (stmt == FieldRef)
        {
          auto argument_node = stmt / Arg;
          code << uleb(
            +(((argument_node / Type) == ArgMove) ? Op::FieldRefMove :
                                                    Op::FieldRefCopy));
          code << dst(stmt) << src(argument_node) << fld(stmt);
        }
        else if (stmt == ArrayRef)
        {
          auto argument_node = stmt / Arg;
          code << uleb(
            +(((argument_node / Type) == ArgMove) ? Op::ArrayRefMove :
                                                    Op::ArrayRefCopy));
          code << dst(stmt) << src(argument_node) << rhs(stmt);
        }
        else if (stmt == ArrayRefConst)
        {
          auto argument_node = stmt / Arg;
          code << uleb(
            +(((argument_node / Type) == ArgMove) ? Op::ArrayRefMoveConst :
                                                    Op::ArrayRefCopyConst));
          code << dst(stmt) << src(argument_node)
                 << uleb(from_chars_sep_v<uint64_t>(stmt / Rhs));
        }
        else if (stmt == Load)
        {
          code << uleb(+Op::Load) << dst(stmt) << rhs(stmt);
        }
        else if (stmt == Store)
        {
          auto argument_node = stmt / Arg;
          code << uleb(
            +(((argument_node / Type) == ArgMove) ? Op::StoreMove :
                                                    Op::StoreCopy));
          code << dst(stmt) << src(stmt) << src(argument_node);
        }
        else if (stmt == Lookup)
        {
          code << uleb(+Op::LookupDynamic) << dst(stmt) << src(stmt)
                 << mth(stmt);
        }
        else if (stmt == Call)
        {
          args(stmt / Args);
          code << uleb(+Op::CallStatic) << dst(stmt) << fn(stmt);
        }
        else if (stmt == MemoSlot)
        {
          auto function_id =
            std::string((stmt / FunctionId)->location().view());
          auto slot = memo_slot_map.find(function_id);
          assert(slot != memo_slot_map.end());
          code << uleb(+Op::MemoLoad) << dst(stmt) << uleb(slot->second);
        }
        else if (stmt == CallDyn)
        {
          args(stmt / Args);
          code << uleb(+Op::CallDynamic) << dst(stmt) << src(stmt);
        }
        else if (stmt == TryCallDyn)
        {
          args(stmt / Args);
          code << uleb(+Op::TryCallDynamic) << dst(stmt)
                 << src(stmt);
        }
        else if (stmt == FFI)
        {
          args(stmt / Args);
          code << uleb(+Op::FFI) << dst(stmt)
                 << uleb(*state.get_symbol_id(stmt / SymbolId));
        }
        else if (stmt == FFIStruct)
        {
          code << uleb(+Op::FFIStruct) << dst(stmt)
                 << uleb(state.type_id(stmt / Type));
        }
        else if (stmt == FFILoad)
        {
          code << uleb(+Op::FFILoad) << dst(stmt) << lhs(stmt)
                 << rhs(stmt)
                 << uleb(*func_state.get_register_id(stmt / Kind))
                 << uleb(state.type_id(stmt / Type));
        }
        else if (stmt == FFIStore)
        {
          code << uleb(+Op::FFIStore) << dst(stmt) << lhs(stmt)
                 << rhs(stmt)
                 << uleb(*func_state.get_register_id(stmt / Kind))
                 << uleb(*func_state.get_register_id(stmt / ValueSrc))
                 << uleb(state.type_id(stmt / Type));
        }
        else if (stmt == ArrayCopy)
        {
          args(stmt / Args);
          code << uleb(+Op::ArrayCopy) << dst(stmt);
        }
        else if (stmt == ArrayFill)
        {
          args(stmt / Args);
          code << uleb(+Op::ArrayFill) << dst(stmt);
        }
        else if (stmt == ArrayCompare)
        {
          args(stmt / Args);
          code << uleb(+Op::ArrayCompare) << dst(stmt);
        }
        else if (stmt == When)
        {
          args(stmt / Args);
          code << uleb(+Op::WhenStatic) << dst(stmt)
                 << uleb(state.type_id(stmt / Cown)) << fn(stmt);
        }
        else if (stmt == WhenDyn)
        {
          args(stmt / Args);
          code << uleb(+Op::WhenDynamic) << dst(stmt)
                 << uleb(state.type_id(stmt / Cown))
                 << src(stmt);
        }
        else if (stmt == Add)
          binary(stmt, Op::Add);
        else if (stmt == Sub)
          binary(stmt, Op::Sub);
        else if (stmt == Mul)
          binary(stmt, Op::Mul);
        else if (stmt == Div)
          binary(stmt, Op::Div);
        else if (stmt == Mod)
          binary(stmt, Op::Mod);
        else if (stmt == Pow)
          binary(stmt, Op::Pow);
        else if (stmt == And)
          binary(stmt, Op::And);
        else if (stmt == Or)
          binary(stmt, Op::Or);
        else if (stmt == Xor)
          binary(stmt, Op::Xor);
        else if (stmt == Shl)
          binary(stmt, Op::Shl);
        else if (stmt == Shr)
          binary(stmt, Op::Shr);
        else if (stmt == Eq)
          binary(stmt, Op::Eq);
        else if (stmt == Ne)
          binary(stmt, Op::Ne);
        else if (stmt == Lt)
          binary(stmt, Op::Lt);
        else if (stmt == Le)
          binary(stmt, Op::Le);
        else if (stmt == Gt)
          binary(stmt, Op::Gt);
        else if (stmt == Ge)
          binary(stmt, Op::Ge);
        else if (stmt == Min)
          binary(stmt, Op::Min);
        else if (stmt == Max)
          binary(stmt, Op::Max);
        else if (stmt == LogBase)
          binary(stmt, Op::LogBase);
        else if (stmt == Atan2)
          binary(stmt, Op::Atan2);
        else if (stmt == Neg)
          unary(stmt, Op::Neg);
        else if (stmt == Not)
          unary(stmt, Op::Not);
        else if (stmt == Abs)
          unary(stmt, Op::Abs);
        else if (stmt == Ceil)
          unary(stmt, Op::Ceil);
        else if (stmt == Floor)
          unary(stmt, Op::Floor);
        else if (stmt == Exp)
          unary(stmt, Op::Exp);
        else if (stmt == Log)
          unary(stmt, Op::Log);
        else if (stmt == Sqrt)
          unary(stmt, Op::Sqrt);
        else if (stmt == Cbrt)
          unary(stmt, Op::Cbrt);
        else if (stmt == IsInf)
          unary(stmt, Op::IsInf);
        else if (stmt == IsNaN)
          unary(stmt, Op::IsNaN);
        else if (stmt == Sin)
          unary(stmt, Op::Sin);
        else if (stmt == Cos)
          unary(stmt, Op::Cos);
        else if (stmt == Tan)
          unary(stmt, Op::Tan);
        else if (stmt == Asin)
          unary(stmt, Op::Asin);
        else if (stmt == Acos)
          unary(stmt, Op::Acos);
        else if (stmt == Atan)
          unary(stmt, Op::Atan);
        else if (stmt == Sinh)
          unary(stmt, Op::Sinh);
        else if (stmt == Cosh)
          unary(stmt, Op::Cosh);
        else if (stmt == Tanh)
          unary(stmt, Op::Tanh);
        else if (stmt == Asinh)
          unary(stmt, Op::Asinh);
        else if (stmt == Acosh)
          unary(stmt, Op::Acosh);
        else if (stmt == Atanh)
          unary(stmt, Op::Atanh);
        else if (stmt == Bits)
          unary(stmt, Op::Bits);
        else if (stmt == Len)
          unary(stmt, Op::Len);
        else if (stmt == MakePtr)
          unary(stmt, Op::Ptr);
        else if (stmt == Read)
          unary(stmt, Op::Read);
        else if (stmt == Const_E)
          code << uleb(+Op::Const_E) << dst(stmt);
        else if (stmt == Const_Pi)
          code << uleb(+Op::Const_Pi) << dst(stmt);
        else if (stmt == Const_Inf)
          code << uleb(+Op::Const_Inf) << dst(stmt);
        else if (stmt == Const_NaN)
          code << uleb(+Op::Const_NaN) << dst(stmt);
        else if (stmt == MakeCallback)
          unary(stmt, Op::MakeCallback);
        else if (stmt == CodePtrCallback)
          unary(stmt, Op::CodePtrCallback);
        else if (stmt == FreeCallback)
          unary(stmt, Op::FreeCallback);
        else if (stmt == Pin)
          unary(stmt, Op::Pin);
        else if (stmt == Unpin)
          unary(stmt, Op::Unpin);
        else if (stmt == Merge)
          binary(stmt, Op::Merge);
        else if (stmt == AddExternal)
          code << uleb(+Op::AddExternal) << dst(stmt);
        else if (stmt == RemoveExternal)
          code << uleb(+Op::RemoveExternal) << dst(stmt);
        else if (stmt == Typetest)
        {
          code << uleb(+Op::Typetest) << dst(stmt) << src(stmt)
                 << uleb(state.type_id(stmt / Type));
        }
        else if (stmt == GetRaise)
          code << uleb(+Op::GetRaise) << dst(stmt);
        else if (stmt == SetRaise)
          unary(stmt, Op::SetRaise);
      }

      void term(Node term)
      {
        if (term == Tailcall)
        {
          args(term / MoveArgs);
          code << uleb(+Op::TailcallStatic) << fn(term);
        }
        else if (term == TailcallDyn)
        {
          args(term / MoveArgs);
          code << uleb(+Op::TailcallDynamic) << dst(term);
        }
        else if (term == Return)
        {
          code << uleb(+Op::Return) << dst(term);
        }
        else if (term == Raise)
        {
          code << uleb(+Op::Raise) << dst(term);
        }
        else if (term == Cond)
        {
          auto true_label = *func_state.get_label_id(term / Lhs);
          auto false_label = *func_state.get_label_id(term / Rhs);
          code << uleb(+Op::Cond) << dst(term) << uleb(true_label)
                 << uleb(false_label);
        }
        else if (term == Jump)
        {
          code << uleb(+Op::Jump)
                 << uleb(*func_state.get_label_id(term / LabelId));
        }
      }
    };
  }

  void encode_statement(
    Compilation& state,
    FuncState& func_state,
    const MemoSlots& memo_slot_map,
    ByteBuffer& code,
    Node stmt)
  {
    InstructionEncoder{state, func_state, memo_slot_map, code}.stmt(
      stmt);
  }

  void encode_terminator(
    Compilation& state,
    FuncState& func_state,
    ByteBuffer& code,
    Node term)
  {
    const MemoSlots no_memo_slots;
    InstructionEncoder{state, func_state, no_memo_slots, code}.term(
      term);
  }
}