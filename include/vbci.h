#pragma once

#include <cstdint>

namespace vbci
{
  // Each instruction is 32 bits, with the first byte being the opcode, followed
  // by 3 8-bit register indices.
  using Code = uint32_t;
  using GlobalId = uint32_t;
  using TypeId = uint32_t;
  using FuncId = uint32_t;
  using FieldId = uint32_t;

  enum class Op : uint8_t
  {
    // Load a global value.
    // Arg0 = dst.
    // Stream: 32 bit global ID.
    Global,

    // Load a primitive value.
    // Arg0 = dst.
    // Arg1 = value type.
    // Arg2 = primitive value if 8 bits or less.
    // Stream: 16-64 bit primitive value if required.
    NewPrimitive,

    // Allocates a new object in the current frame. Arguments are invalidated.
    // Arg0 = dst.
    // Arg1 = U8 argument base.
    // Stream: 32 bit type ID.
    NewStack,

    // Allocates a new object in the same region. Arguments are invalidated.
    // Arg0 = dst.
    // Arg1 = U8 argument base.
    // Arg2 = object in the target region.
    // Stream: 32 bit type ID.
    NewHeap,

    // Allocates a new object in a new region. Arguments are invalidated.
    // Arg0 = dst.
    // Arg1 = argument base.
    // Arg2 = region type.
    // Stream: 32 bit type ID.
    NewRegion,

    // Copies the value.
    // Arg0 = dst.
    // Arg1 = src.
    Copy,

    // Moves the value, invalidating the source.
    // Arg0 = dst.
    // Arg1 = src.
    Move,

    // Invalidates the value.
    // Arg0 = dst.
    Drop,

    // Creates a reference to a field in a target object.
    // Arg0 = dst.
    // Arg1 = src.
    // Stream: 32 bit field ID.
    Ref,

    // Loads a value from a reference.
    // Arg0 = dst.
    // Arg1 = src reference.
    Load,

    // Stores a value in a reference. The previous value is stored in dst.
    // Arg0 = dst.
    // Arg1 = reference.
    // Arg2 = src.
    Store,

    // Creates a function pointer to a method in the target object.
    // Arg0 = dst.
    // Arg1 = src object.
    // Stream: 32 bit function ID.
    Lookup,

    // Calls the function with the specified ID. Arguments are invalidated. A
    // dynamic call looks up the method in the first argument. A tail call will
    // replace the current frame. A function call will immediately return a
    // Raise as a Return, and a Throw as a Throw. A block call will return a
    // Raise as a Raise and a THrow as a Throw. A try call will not return a
    // non-local return.
    // Arg0 = dst.
    // Arg1 = argument base.
    // Arg2 = call type: function static, function dynamic, block static, block
    // dynamic, try static, try dynamic, tail call static, tail call dynamic.
    // Stream: 32 bit function ID.
    Call,

    // Return from the current function, as a local return (Return), a non-local
    // return (Raise) or an exception (Throw).
    // Arg0 = return value.
    // Arg1 = non-local status: Return, Raise, or Throw.
    Return,

    // TODO: how does this work?
    Conditional,

    // Binary operators.
    // Arg0 = dst.
    // Arg1 = left-hand side src.
    // Arg2 = right-hand side src.
    Add,
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
    Atan2,

    // Math operator.
    // Arg0 = dst.
    // Arg1 = src.
    // Arg2 = math operator.
    MathOp,

    // Converts src to the specified type.
    // Arg0 = dst.
    // Arg1 = src.
    // Arg2 = target ValueType.
    Convert,

    COUNT,
  };

  enum class MathOp : uint32_t
  {
    Neg,
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

    // Constants don't use the src argument.
    // TODO: numerical limits? specify a type for limits and constants?
    Const_E,
    Const_Pi,
    Const_Inf,
    Const_NaN,
  };

  // typetest
  // merge, freeze, extract
  // when

  // need arrays. implement strings as arrays?
  // loops as tailcalls?
  // stdio? FFI for files, sockets, etc? entropy?
  // some kind of dlopen system for adding FFI?

  enum class ValueType : uint8_t
  {
    None,
    Bool,
    I8,
    I16,
    I32,
    I64,
    U8,
    U16,
    U32,
    U64,
    F32,
    F64,
    Object,
    Cown,
    Ref,
    CownRef,
    Function,
    Error,
    Invalid,
  };

  enum class RegionType : uint8_t
  {
    RegionRC,
    RegionGC,
    RegionArena
  };

  enum class CallType : uint8_t
  {
    FunctionStatic,
    FunctionDynamic,
    BlockStatic,
    BlockDynamic,
    TryStatic,
    TryDynamic,
    TailCallStatic,
    TailCallDynamic,
  };

  enum class Condition : uint8_t
  {
    Return,
    Raise,
    Throw,
  };
}
