#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace vbci
{
  // Each instruction is 32 bits, with the first byte being the opcode, followed
  // by 3 8-bit register indices.
  using Code = uint32_t;
  using Id = uint32_t;

  inline const auto MagicNumber = Code(0xDEC0ADDE);
  inline const auto CurrentVersion = Code(0);
  inline const auto MaxRegisters = size_t(256);
  inline const auto MainFuncId = Id(0);
  inline const auto FinalMethodId = Id(0);

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
    Const,

    // Converts src to the specified type.
    // Arg0 = dst.
    // Arg1 = destination ValueType.
    // Arg2 = src.
    Convert,

    // Allocates a new object in the current frame. Fields are initialized from
    // arguments.
    // Arg0 = dst.
    // Stream: 32 bit class ID.
    Stack,

    // Allocates a new object in the same region. Fields are initialized from
    // arguments.
    // Arg0 = dst.
    // Arg1 = allocation in the target region.
    // Stream: 32 bit class ID.
    Heap,

    // Allocates a new object in a new region. Fields are initialized from
    // arguments.
    // Arg0 = dst.
    // Arg1 = region type.
    // Stream: 32 bit class ID.
    Region,

    // Allocates a new array in the current frame. The array is uninitialized.
    // Arg0 = dst.
    // Arg1 = size.
    // Stream: 32 bit content type ID.
    StackArray,

    // Allocates a new array in the current frame. The array is uninitialized.
    // Arg0 = dst.
    // Stream: 32 bit content type ID, 64 bit static size.
    StackArrayConst,

    // Allocates a new array in the same region. The array is uninitialized.
    // Arg0 = dst.
    // Arg1 = allocation in the target region.
    // Arg2 = size.
    // Stream: 32 bit content type ID.
    HeapArray,

    // Allocates a new array in the same region. The array is uninitialized.
    // Arg0 = dst.
    // Arg1 = allocation in the target region.
    // Stream: 32 bit content type ID, 64 bit static size.
    HeapArrayConst,

    // Allocates a new array in a new region. The array is uninitialized.
    // Arg0 = dst.
    // Arg1 = region type.
    // Arg2 = size.
    // Stream: 32 bit content type ID.
    RegionArray,

    // Allocates a new array in a new region. The array is uninitialized.
    // Arg0 = dst.
    // Arg1 = region type.
    // Stream: 32 bit content type ID, 64 bit static size.
    RegionArrayConst,

    // Copies the value.
    // Arg0 = dst.
    // Arg1 = src.
    Copy,

    // Moves the value, invalidating the source.
    // Arg0 = dst.
    // Arg1 = src.
    Move,

    // Invalidates the value. This allows for prompt deallocation.
    // Arg0 = dst.
    Drop,

    // Creates a reference to a field in a target object.
    // Arg0 = dst.
    // Arg1 = move or copy the source.
    // Arg2 = src.
    // Stream: 32 bit field ID.
    Ref,

    // Creates a reference to an array slot, moving the source.
    // Arg0 = dst.
    // Arg1 = src.
    // Arg2 = index.
    ArrayRefMove,

    // Creates a reference to an array slot, copying the source.
    // Arg0 = dst.
    // Arg1 = src.
    // Arg2 = index.
    ArrayRefCopy,

    // Creates a reference to an array slot from a constant index.
    // Arg0 = dst.
    // Arg1 = move or copy.
    // Arg2 = src.
    // Stream: 64 bit index.
    ArrayRefConst,

    // Loads a value from a reference.
    // Arg0 = dst.
    // Arg1 = src reference.
    Load,

    // Moves a value into a reference. The previous value is moved into dst.
    // Arg0 = dst.
    // Arg1 = reference.
    // Arg2 = src.
    StoreMove,

    // Copies a value into a reference. The previous value is moved into dst.
    // Arg0 = dst.
    // Arg1 = reference.
    // Arg2 = src.
    StoreCopy,

    // Creates a function pointer. For a static call, src is ignored.
    // For a dynamic call, the method is looked up in the src object.
    // Arg0 = dst.
    // Arg1 = call type: function static or function dynamic.
    // Arg2 = src, ignored if static.
    // Stream: 32 bit function ID (static) or method ID (dynamic).
    Lookup,

    // Set a value as an argument index in the next frame. Use this to set up
    // the arguments for an object allocation or a function call. Arguments are
    // set up in order, and cleared on a call, tailcall, ore return.
    // Arg0 = move or copy.
    // Arg1 = src.
    Arg,

    // Arg0 = dst.
    // Arg1 = call type: function static, function dynamic, block static, block
    // dynamic, try static, try dynamic, FFI.
    // Arg2 = function value, ignored if static.
    // Stream: 32 bit function ID, for static calls.
    Call,

    // Replace the current frame with a new one.
    // Arg0 = call type: function static or function dynamic.
    // Arg1 = function value, ignored if static.
    // Stream: 32 bit function ID, for static tail calls.
    Tailcall,

    // Return from the current function, as a local return (Return), a non-local
    // return (Raise) or an exception (Throw).
    // Arg0 = return value.
    // Arg1 = non-local status: Return, Raise, or Throw.
    Return,

    // Jump to a label depending on a boolean condition.
    // Arg0 = condition.
    // Arg1 = on-true label.
    // Arg2 = on-false label.
    Cond,

    // Jump to a label.
    // Arg0 = label.
    Jump,

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
    // Arg1 = math operator.
    // Arg2 = src.
    MathOp,

    COUNT,
  };

  enum class MathOp : uint32_t
  {
    // Unary operators.
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
    Const_E,
    Const_Pi,
    Const_Inf,
    Const_NaN,
  };

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
    ILong,
    ULong,
    ISize,
    USize,
    Ptr,
    Object,
    Array,
    Cown,
    Ref,
    ArrayRef,
    CownRef,
    Function,
    Error,
    Invalid,
  };

  inline const auto NumPrimitiveClasses =
    static_cast<size_t>(ValueType::F64) + 1;

  enum class RegionType : uint8_t
  {
    RegionRC,
    RegionGC,
    RegionArena
  };

  enum class ArgType : uint8_t
  {
    Move,
    Copy,
  };

  enum class CallType : uint8_t
  {
    CallStatic,
    CallDynamic,
    SubcallStatic,
    SubcallDynamic,
    TryStatic,
    TryDynamic,
    FFI,
  };

  enum class Condition : uint8_t
  {
    Return,
    Raise,
    Throw,
  };

  enum class DIOp : uint8_t
  {
    File,
    Offset,
    Skip,
  };

  inline constexpr uint8_t operator+(Op op)
  {
    return static_cast<uint8_t>(op);
  }

  inline constexpr uint8_t operator+(MathOp op)
  {
    return static_cast<uint8_t>(op);
  }

  inline constexpr uint8_t operator+(ValueType v)
  {
    return static_cast<uint8_t>(v);
  }

  inline constexpr uint8_t operator+(RegionType r)
  {
    return static_cast<uint8_t>(r);
  }

  inline constexpr uint8_t operator+(ArgType a)
  {
    return static_cast<uint8_t>(a);
  }

  inline constexpr uint8_t operator+(CallType c)
  {
    return static_cast<uint8_t>(c);
  }

  inline constexpr uint8_t operator+(Condition c)
  {
    return static_cast<uint8_t>(c);
  }

  inline constexpr uint8_t operator+(DIOp c)
  {
    return static_cast<uint8_t>(c);
  }

  namespace type
  {
    inline const auto Shift = size_t(3);
    inline const auto Mask = (size_t(1) << Shift) - 1;
    inline const auto Max =
      (size_t(1) << ((sizeof(Id) * 8) - Shift)) - (+ValueType::Ptr + 1);

    enum class Mod : uint8_t
    {
      Array = 1 << 0,
      Ref = 1 << 1,
      Cown = 1 << 2,
    };

    inline constexpr uint8_t operator+(Mod t)
    {
      return static_cast<uint8_t>(t);
    }

    inline constexpr bool is_mod(Id type_id)
    {
      return (type_id & Mask);
    }

    inline constexpr bool is_array(Id type_id)
    {
      return (type_id & +Mod::Array);
    }

    inline constexpr bool is_ref(Id type_id)
    {
      return (type_id & +Mod::Ref);
    }

    inline constexpr bool is_dyn(Id type_id)
    {
      return !is_mod(type_id) && (type_id == 0);
    }

    inline constexpr bool is_cown(Id type_id)
    {
      return (type_id & +Mod::Cown);
    }

    inline constexpr bool is_def(size_t num_classes, Id type_id)
    {
      return !is_mod(type_id) &&
        ((type_id >> Shift) >= (+ValueType::Ptr + 2 + num_classes));
    }

    inline constexpr size_t def_idx(size_t num_classes, Id type_id)
    {
      assert(is_def(num_classes, type_id));
      return (type_id >> Shift) - (+ValueType::Ptr + 2 + num_classes);
    }

    inline constexpr Id dyn()
    {
      return 0;
    }

    inline constexpr Id val(ValueType t)
    {
      return (+t + 1) << Shift;
    }

    inline constexpr Id cls(Id class_id)
    {
      assert(class_id <= Max);
      return (+ValueType::Ptr + 2 + class_id) << Shift;
    }

    inline constexpr Id def(size_t num_classes, Id typedef_id)
    {
      assert((num_classes + typedef_id) <= Max);
      return (+ValueType::Ptr + 2 + num_classes + typedef_id) << Shift;
    }

    inline constexpr Id array(Id type_id)
    {
      assert(!is_array(type_id));
      return type_id | +Mod::Array;
    }

    inline constexpr Id ref(Id type_id)
    {
      assert(!is_ref(type_id));
      return type_id | +Mod::Ref;
    }

    inline constexpr Id cown(Id type_id)
    {
      assert(!is_cown(type_id));
      return type_id | +Mod::Cown;
    }

    inline constexpr ValueType val(Id type_id)
    {
      if (type_id & Mask)
        return ValueType::Invalid;

      type_id = (type_id >> Shift) - 1;

      if (type_id > +ValueType::Ptr)
        return ValueType::Invalid;

      return static_cast<ValueType>(type_id);
    }

    inline constexpr bool too_many(size_t num_classes, size_t num_typedefs)
    {
      return (num_classes + num_typedefs) <= Max;
    }
  }
}
