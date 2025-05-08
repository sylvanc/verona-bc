#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace vbci
{
  using Id = uint32_t;

  inline const auto MagicNumber = size_t(0xDEC0ADDE);
  inline const auto CurrentVersion = size_t(0);
  inline const auto MainFuncId = size_t(0);
  inline const auto FinalMethodId = size_t(0);
  inline const auto ApplyMethodId = size_t(1);

  // Op codes are ULEB128 encoded. Arguments are ULEB128 encoded unless they're
  // known to be signed integers (zigzag SLEB128) or floats (bitcast zigzag
  // ULEB128).
  enum class Op
  {
    // Load a global value.
    // Arg0 = dst.
    // Arg1 = global ID.
    Global,

    // Load a primitive value.
    // Arg0 = dst.
    // Arg1 = value type.
    // Arg2 = primitive value.
    Const,

    // Converts src to the specified type.
    // Arg0 = dst.
    // Arg1 = target value type.
    // Arg2 = src.
    Convert,

    // Allocates a new object in the current frame. Fields are initialized from
    // arguments.
    // Arg0 = dst.
    // Arg1 = class ID.
    Stack,

    // Allocates a new object in the same region. Fields are initialized from
    // arguments.
    // Arg0 = dst.
    // Arg1 = allocation in the target region.
    // Arg2 = class ID.
    Heap,

    // Allocates a new object in a new region. Fields are initialized from
    // arguments.
    // Arg0 = dst.
    // Arg1 = region type.
    // Arg2 = class ID.
    Region,

    // Allocates a new array in the current frame. The array is uninitialized.
    // Arg0 = dst.
    // Arg1 = size.
    // Arg2 = content type ID.
    StackArray,

    // Allocates a new array in the current frame. The array is uninitialized.
    // Arg0 = dst.
    // Arg1 = content type ID.
    // Arg2 = constant size.
    StackArrayConst,

    // Allocates a new array in the same region. The array is uninitialized.
    // Arg0 = dst.
    // Arg1 = allocation in the target region.
    // Arg2 = size.
    // Arg3 = content type ID.
    HeapArray,

    // Allocates a new array in the same region. The array is uninitialized.
    // Arg0 = dst.
    // Arg1 = allocation in the target region.
    // Arg2 = content type ID.
    // Arg3 = constant size.
    HeapArrayConst,

    // Allocates a new array in a new region. The array is uninitialized.
    // Arg0 = dst.
    // Arg1 = region type.
    // Arg2 = size.
    // Arg3 = content type ID.
    RegionArray,

    // Allocates a new array in a new region. The array is uninitialized.
    // Arg0 = dst.
    // Arg1 = region type.
    // Arg2 = content type ID.
    // Arg3 = constant size.
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

    // Creates a reference to a field in a target object, moving the source.
    // Arg0 = dst.
    // Arg1 = src.
    // Arg2 = field ID.
    RefMove,

    // Creates a reference to a field in a target object, copying the source.
    // Arg0 = dst.
    // Arg1 = src.
    // Arg2 = field ID.
    RefCopy,

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

    // Creates a reference to an array slot from a constant index, moving the
    // source.
    // Arg0 = dst.
    // Arg1 = src.
    // Arg2 = constant index.
    ArrayRefMoveConst,

    // Creates a reference to an array slot from a constant index, copying the
    // source.
    // Arg0 = dst.
    // Arg1 = src.
    // Arg2 = constant index.
    ArrayRefCopyConst,

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

    // Creates a function pointer from a function ID.
    // Arg0 = dst.
    // Arg1 = function ID.
    LookupStatic,

    // Creates a function pointer from a source and a method.
    // Arg0 = dst.
    // Arg1 = src.
    // Arg2 = method ID.
    LookupDynamic,

    // Set a value as an argument index in the next frame. Use this to set up
    // the arguments for an object allocation or a function call. Arguments are
    // set up in order, and cleared on a call, tailcall, ore return.
    // Arg0 = src.
    ArgMove,
    ArgCopy,

    // Arg0 = dst.
    // Arg1 = function ID.
    CallStatic,

    // Arg0 = dst.
    // Arg1 = function pointer.
    CallDynamic,

    // Arg0 = dst.
    // Arg1 = function ID.
    SubcallStatic,

    // Arg0 = dst.
    // Arg1 = function pointer.
    SubcallDynamic,

    // Arg0 = dst.
    // Arg1 = function ID.
    TryStatic,

    // Arg0 = dst.
    // Arg1 = function pointer.
    TryDynamic,

    // Arg0 = dst.
    // Arg1 = symbol ID.
    FFI,

    // Set up the arguments with the behavior closure or function pointer first,
    // followed by the cowns to be acquired. The behavior closure must have an
    // @apply method where the implementation takes the behavior and the
    // acquired cowns as arguments. With a function pointer, it takes just the
    // acquired cowns.
    // Arg0 = dst.
    When,

    // Returns true if the dynamic type of src is a subtype of the type ID.
    // Arg0 = dst.
    // Arg1 = src.
    // Arg2 = type ID.
    Typetest,

    // Replace the current frame with a new one.
    // Arg0 = function ID.
    TailcallStatic,

    // Replace the current frame with a new one.
    // Arg0 = function pointer.
    TailcallDynamic,

    // Return, raise, or throw from the current function.
    // Arg0 = return value.
    Return,
    Raise,
    Throw,

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

    // Unary operators.
    // Arg0 = dst.
    // Arg1 = src.
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

    Len,
    ArrayPtr,
    StructPtr,

    // This creates a read-only view of the target. Currently, this can only be
    // done on cowns.
    Read,

    // Constants.
    // Arg0 = dst.
    Const_E,
    Const_Pi,
    Const_Inf,
    Const_NaN,
  };

  enum class ValueType
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
    ILong,
    ULong,
    ISize,
    USize,
    F32,
    F64,
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
    static_cast<size_t>(ValueType::Ptr) + 1;

  enum class RegionType
  {
    RegionRC,
    RegionGC,
    RegionArena
  };

  enum class Condition
  {
    Return,
    Raise,
    Throw,
  };

  enum class DIOp
  {
    File,
    Offset,
    Skip,
  };

  inline constexpr size_t operator+(Op op)
  {
    return static_cast<size_t>(op);
  }

  inline constexpr size_t operator+(ValueType v)
  {
    return static_cast<size_t>(v);
  }

  inline constexpr size_t operator+(RegionType r)
  {
    return static_cast<size_t>(r);
  }

  inline constexpr size_t operator+(Condition c)
  {
    return static_cast<size_t>(c);
  }

  inline constexpr size_t operator+(DIOp c)
  {
    return static_cast<size_t>(c);
  }

  namespace type
  {
    inline const auto Shift = Id(3);
    inline const auto Mask = (Id(1) << Shift) - 1;

    enum class Mod
    {
      Array = 1 << 0,
      Ref = 1 << 1,
      Cown = 1 << 2,
    };

    inline constexpr Id operator+(Mod t)
    {
      return static_cast<Id>(t);
    }

    inline constexpr bool is_mod(Id type_id)
    {
      return (type_id & Mask);
    }

    inline constexpr Id mod(Id type_id)
    {
      return type_id & Mask;
    }

    inline constexpr Id no_mod(Id type_id)
    {
      return type_id & ~Mask;
    }

    inline constexpr bool is_dyn(Id type_id)
    {
      return type_id == 0;
    }

    inline constexpr bool is_val(Id type_id)
    {
      auto t = type_id >> Shift;
      return !is_mod(type_id) && (t > 0) && (t <= NumPrimitiveClasses);
    }

    inline constexpr bool is_cls(size_t num_classes, Id type_id)
    {
      auto t = type_id >> Shift;
      return !is_mod(type_id) && (t > NumPrimitiveClasses) &&
        (t <= (NumPrimitiveClasses + num_classes));
    }

    inline constexpr bool is_def(size_t num_classes, Id type_id)
    {
      return !is_mod(type_id) &&
        ((type_id >> Shift) > (NumPrimitiveClasses + num_classes));
    }

    inline constexpr bool is_array(Id type_id)
    {
      // An array of anything is an array.
      return (type_id & +Mod::Array);
    }

    inline constexpr bool is_ref(Id type_id)
    {
      // A ref can't be an array.
      return !is_array(type_id) && (type_id & +Mod::Ref);
    }

    inline constexpr bool is_cown(Id type_id)
    {
      // A cown can't be a ref or an array.
      return (type_id & Mask) == +Mod::Cown;
    }

    inline constexpr ValueType val(Id type_id)
    {
      assert(is_val(type_id));
      return static_cast<ValueType>((type_id >> Shift) - 1);
    }

    inline constexpr size_t cls_idx(Id type_id)
    {
      return (type_id >> Shift) - (NumPrimitiveClasses + 1);
    }

    inline constexpr size_t def_idx(size_t num_classes, Id type_id)
    {
      assert(is_def(num_classes, type_id));
      return (type_id >> Shift) - (NumPrimitiveClasses + num_classes + 1);
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
      return (NumPrimitiveClasses + class_id + 1) << Shift;
    }

    inline constexpr Id def(size_t num_classes, Id typedef_id)
    {
      return (NumPrimitiveClasses + num_classes + typedef_id + 1) << Shift;
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

    inline constexpr Id uncown(Id type_id)
    {
      assert(is_cown(type_id));
      return type_id & ~+Mod::Cown;
    }
  }
}
