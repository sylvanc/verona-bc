#include "value.h"

#include "array.h"
#include "cown.h"
#include "object.h"
#include "platform.h"
#include "program.h"

namespace vbci
{
  Value::Value() : tag(ValueType::Invalid) {}
  Value::Value(bool b) : b(b), tag(ValueType::Bool) {}
  Value::Value(uint8_t u8) : u8(u8), tag(ValueType::U8) {}
  Value::Value(uint16_t u16) : u16(u16), tag(ValueType::U16) {}
  Value::Value(uint32_t u32) : u32(u32), tag(ValueType::U32) {}
  Value::Value(uint64_t u64) : u64(u64), tag(ValueType::U64) {}
  Value::Value(int8_t i8) : i8(i8), tag(ValueType::I8) {}
  Value::Value(int16_t i16) : i16(i16), tag(ValueType::I16) {}
  Value::Value(int32_t i32) : i32(i32), tag(ValueType::I32) {}
  Value::Value(int64_t i64) : i64(i64), tag(ValueType::I64) {}
#ifdef PLATFORM_IS_MACOSX
  Value::Value(long ilong) : ilong(ilong), tag(ValueType::ILong) {}
  Value::Value(unsigned long ulong) : ulong(ulong), tag(ValueType::ULong) {}
#endif
  Value::Value(float f32) : f32(f32), tag(ValueType::F32) {}
  Value::Value(double f64) : f64(f64), tag(ValueType::F64) {}
  Value::Value(void* ptr) : ptr(ptr), tag(ValueType::Ptr) {}
  Value::Value(Object* obj) : obj(obj), tag(ValueType::Object), readonly(0) {}

  Value::Value(Object* obj, bool ro)
  : obj(obj), tag(ValueType::Object), readonly(ro)
  {}

  Value::Value(Array* arr) : arr(arr), tag(ValueType::Array), readonly(0) {}
  Value::Value(Cown* cown) : cown(cown), tag(ValueType::Cown) {}

  Value::Value(Register& val, size_t frame)
  : reg(&val), idx(frame), tag(ValueType::RegisterRef), readonly(0)
  {}

  Value::Value(Object* obj, size_t f, bool ro)
  : obj(obj), idx(f), tag(ValueType::FieldRef), readonly(ro)
  {}

  Value::Value(Array* arr, size_t idx, bool ro)
  : arr(arr), idx(idx), tag(ValueType::ArrayRef), readonly(ro)
  {}

  Value::Value(Cown* cown, bool ro)
  : cown(cown), tag(ValueType::CownRef), readonly(ro)
  {}

  Value::Value(Error error) : tag(ValueType::Error)
  {
    err.error = error;
    auto [func, pc] = Thread::debug_info();
    err.func = uintptr_t(func);
    idx = pc;
  }

  Value::Value(Function* func) : func(func), tag(ValueType::Function)
  {
    if (!func)
      Value::error(Error::MethodNotFound);
  }

  Value Value::copy_value() const
  {
    Value v;
    std::memcpy(static_cast<void*>(&v), this, sizeof(Value));
    v.inc<false>();
    return v;
  }

  Register Value::copy_reg() const
  {
    Value v;
    std::memcpy(static_cast<void*>(&v), this, sizeof(Value));
    // Performed the required increment for moving into a register
    v.inc<true>();
    return Register(std::move(v));
  }

  Value::Value(Value&& that) noexcept
  {
    std::memcpy(static_cast<void*>(this), &that, sizeof(Value));
    that.tag = ValueType::Invalid;
  }

  Value& Value::operator=(Value&& that) noexcept
  {
    if (this == &that)
      return *this;

    // This is not a register, Register overrides this behaviour.
    dec<false>();
    std::memcpy(static_cast<void*>(this), &that, sizeof(Value));
    that.tag = ValueType::Invalid;
    return *this;
  }

  Value Value::none()
  {
    return Value(ValueType::None);
  }

  Value Value::null()
  {
    return Value(static_cast<void*>(nullptr));
  }

  ValueType Value::type() const
  {
    return tag;
  }

  Value Value::from_ffi(ValueType t, uint64_t v)
  {
    Value value(t);
    value.u64 = v;
    return value;
  }

  Register Value::from_addr(ValueType t, void* v)
  {
    Register value(t);

    switch (t)
    {
      case ValueType::None:
        break;

      case ValueType::Bool:
        value.b = *reinterpret_cast<bool*>(v);
        break;

      case ValueType::I8:
        value.i8 = *reinterpret_cast<int8_t*>(v);
        break;

      case ValueType::I16:
        value.i16 = *reinterpret_cast<int16_t*>(v);
        break;

      case ValueType::I32:
        value.i32 = *reinterpret_cast<int32_t*>(v);
        break;

      case ValueType::I64:
        value.i64 = *reinterpret_cast<int64_t*>(v);
        break;

      case ValueType::U8:
        value.u8 = *reinterpret_cast<uint8_t*>(v);
        break;

      case ValueType::U16:
        value.u16 = *reinterpret_cast<uint16_t*>(v);
        break;

      case ValueType::U32:
        value.u32 = *reinterpret_cast<uint32_t*>(v);
        break;

      case ValueType::U64:
        value.u64 = *reinterpret_cast<uint64_t*>(v);
        break;

      case ValueType::ILong:
        value.ilong = *reinterpret_cast<long*>(v);
        break;

      case ValueType::ULong:
        value.ulong = *reinterpret_cast<unsigned long*>(v);
        break;

      case ValueType::ISize:
        value.isize = *reinterpret_cast<ssize_t*>(v);
        break;

      case ValueType::USize:
        value.usize = *reinterpret_cast<size_t*>(v);
        break;

      case ValueType::F32:
        value.f32 = *reinterpret_cast<float*>(v);
        break;

      case ValueType::F64:
        value.f64 = *reinterpret_cast<double*>(v);
        break;

      case ValueType::Ptr:
        value.ptr = *reinterpret_cast<void**>(v);
        break;

      case ValueType::Object:
        // Object pointers are stored as one past the object, pointing to the
        // fields, for FFI compatibility.
        value.obj = *reinterpret_cast<Object**>(v);
        if (value.obj == nullptr)
          value.tag = ValueType::Invalid;
        else
          value.obj--;
        break;

      case ValueType::Array:
        // Array pointers are stored as one past the array, pointing to the
        // elements, for FFI compatibility.
        value.arr = *reinterpret_cast<Array**>(v);
        if (value.arr == nullptr)
          value.tag = ValueType::Invalid;
        else
          value.arr--;
        break;

      default:
        std::memcpy(static_cast<void*>(&value), v, sizeof(Value));
        break;
    }

    return value;
  }

  template<bool is_move>
  void Value::to_addr(ValueType t, void* v) const
  {
    switch (t)
    {
      case ValueType::None:
        break;

      case ValueType::Bool:
        *reinterpret_cast<bool*>(v) = b;
        break;

      case ValueType::I8:
        *reinterpret_cast<int8_t*>(v) = i8;
        break;

      case ValueType::I16:
        *reinterpret_cast<int16_t*>(v) = i16;
        break;

      case ValueType::I32:
        *reinterpret_cast<int32_t*>(v) = i32;
        break;

      case ValueType::I64:
        *reinterpret_cast<int64_t*>(v) = i64;
        break;

      case ValueType::U8:
        *reinterpret_cast<uint8_t*>(v) = u8;
        break;

      case ValueType::U16:
        *reinterpret_cast<uint16_t*>(v) = u16;
        break;

      case ValueType::U32:
        *reinterpret_cast<uint32_t*>(v) = u32;
        break;

      case ValueType::U64:
        *reinterpret_cast<uint64_t*>(v) = u64;
        break;

      case ValueType::ILong:
        *reinterpret_cast<long*>(v) = ilong;
        break;

      case ValueType::ULong:
        *reinterpret_cast<unsigned long*>(v) = ulong;
        break;

      case ValueType::ISize:
        *reinterpret_cast<ssize_t*>(v) = isize;
        break;

      case ValueType::USize:
        *reinterpret_cast<size_t*>(v) = usize;
        break;

      case ValueType::F32:
        *reinterpret_cast<float*>(v) = f32;
        break;

      case ValueType::F64:
        *reinterpret_cast<double*>(v) = f64;
        break;

      case ValueType::Ptr:
        *reinterpret_cast<void**>(v) = ptr;
        break;

      case ValueType::Object:
        // Object pointers are stored as one past the object, pointing to the
        // fields, for FFI compatibility.
        *reinterpret_cast<Object**>(v) = (obj + 1);
        break;

      case ValueType::Array:
        // Array pointers are stored as one past the array, pointing to the
        // elements, for FFI compatibility.
        *reinterpret_cast<Array**>(v) = (arr + 1);
        break;

      default:
        std::memcpy(v, this, sizeof(Value));
        break;
    }

    if constexpr (is_move)
    {
      // Clear tag as this value has been invalidated by the move.
      // The const annotation was only required for the copy version,
      // so the const_cast is safe here.
      const_cast<Value*>(this)->tag = ValueType::Invalid;
    }
  }

  // Create the two specialisations of to_addr.
  template void Value::to_addr<true>(ValueType t, void* v) const;
  template void Value::to_addr<false>(ValueType t, void* v) const;

  uint32_t Value::type_id() const
  {
    switch (tag)
    {
      case ValueType::Object:
        return obj->get_type_id();

      case ValueType::Array:
        return arr->get_type_id();

      case ValueType::Cown:
        return cown->get_type_id();

      case ValueType::RegisterRef:
        return Program::get().ref(reg->type_id());

      case ValueType::FieldRef:
        return Program::get().ref(obj->field_type_id(idx));

      case ValueType::ArrayRef:
        return Program::get().ref(arr->content_type_id());

      case ValueType::CownRef:
        return Program::get().ref(cown->content_type_id());

      // Return dyn as the type id for function pointers.
      case ValueType::Function:
        return DynId;

      // Return dyn as the type id for errors.
      case ValueType::Error:
      case ValueType::Invalid:
        return DynId;

      default:
        return +tag;
    }
  }

  bool Value::is_invalid() const
  {
    return tag == ValueType::Invalid;
  }

  bool Value::is_readonly() const
  {
    return readonly;
  }

  bool Value::is_header() const
  {
    switch (tag)
    {
      case ValueType::Object:
      case ValueType::Array:
        return true;

      default:
        return false;
    }
  }

  bool Value::is_function() const
  {
    return tag == ValueType::Function;
  }

  bool Value::is_sendable() const
  {
    switch (tag)
    {
      case ValueType::Object:
        return obj->sendable();

      case ValueType::Array:
        return arr->sendable();

      case ValueType::Cown:
        return true;

      case ValueType::Ptr:
      case ValueType::RegisterRef:
      case ValueType::FieldRef:
      case ValueType::ArrayRef:
      case ValueType::CownRef:
        return false;

      default:
        return true;
    }
  }

  bool Value::is_cown() const
  {
    return tag == ValueType::Cown;
  }

  bool Value::is_error() const
  {
    return tag == ValueType::Error;
  }

  bool Value::get_bool() const
  {
    if (tag != ValueType::Bool)
      Value::error(Error::BadConversion);

    return b;
  }

  int32_t Value::get_i32() const
  {
    if (tag != ValueType::I32)
      Value::error(Error::BadConversion);

    return i32;
  }

  Cown* Value::get_cown() const
  {
    if (tag != ValueType::Cown)
      Value::error(Error::BadConversion);

    return cown;
  }

  Header* Value::get_header() const
  {
    switch (tag)
    {
      case ValueType::Object:
        return obj;

      case ValueType::Array:
        return arr;

      default:
        Value::error(Error::BadConversion);
    }
  }

  Function* Value::function() const
  {
    if (tag != ValueType::Function)
      return nullptr;

    return func;
  }

  size_t Value::get_size() const
  {
    if (tag != ValueType::USize)
      Value::error(Error::BadConversion);

    return usize;
  }

  void* Value::to_ffi()
  {
    switch (tag)
    {
      case ValueType::None:
        return nullptr;

      case ValueType::Bool:
        return &b;

      case ValueType::I8:
        return &i8;

      case ValueType::I16:
        return &i16;

      case ValueType::I32:
        return &i32;

      case ValueType::I64:
        return &i64;

      case ValueType::U8:
        return &u8;

      case ValueType::U16:
        return &u16;

      case ValueType::U32:
        return &u32;

      case ValueType::U64:
        return &u64;

      case ValueType::F32:
        return &f32;

      case ValueType::F64:
        return &f64;

      case ValueType::ILong:
        return &ilong;

      case ValueType::ULong:
        return &ulong;

      case ValueType::ISize:
        return &isize;

      case ValueType::USize:
        return &usize;

      case ValueType::Ptr:
        return &ptr;

      case ValueType::Object:
        return &obj;

      case ValueType::Array:
        return &arr;

      case ValueType::Cown:
        return &cown;

      default:
        return this;
    }
  }

  Value Value::op_bits() const
  {
    switch (tag)
    {
      case ValueType::None:
      case ValueType::Bool:
      case ValueType::I8:
      case ValueType::U8:
        return convert(ValueType::U8);

      case ValueType::I16:
      case ValueType::U16:
        return convert(ValueType::U16);

      case ValueType::I32:
      case ValueType::U32:
        return convert(ValueType::U32);

      case ValueType::I64:
      case ValueType::U64:
        return convert(ValueType::U64);

      case ValueType::ILong:
      case ValueType::ULong:
        return convert(ValueType::ULong);

      case ValueType::F32:
        return Value(ValueType::U32, std::bit_cast<uint32_t>(f32));

      case ValueType::F64:
        return Value(ValueType::U64, std::bit_cast<uint64_t>(f64));

      default:
        return convert(ValueType::USize);
    }
  }

  Value Value::op_len() const
  {
    if (tag == ValueType::Array)
      return Value(ValueType::USize, arr->get_size());

    Value::error(Error::BadOperand);
  }

  Value Value::op_ptr()
  {
    switch (tag)
    {
      case ValueType::None:
        return Value(static_cast<void*>(nullptr));
      case ValueType::Bool:
        return Value(&b);
      case ValueType::I8:
        return Value(&i8);
      case ValueType::I16:
        return Value(&i16);
      case ValueType::I32:
        return Value(&i32);
      case ValueType::I64:
        return Value(&i64);
      case ValueType::U8:
        return Value(&u8);
      case ValueType::U16:
        return Value(&u16);
      case ValueType::U32:
        return Value(&u32);
      case ValueType::U64:
        return Value(&u64);
      case ValueType::F32:
        return Value(&f32);
      case ValueType::F64:
        return Value(&f64);
      case ValueType::ILong:
        return Value(&ilong);
      case ValueType::ULong:
        return Value(&ulong);
      case ValueType::ISize:
        return Value(&isize);
      case ValueType::USize:
        return Value(&usize);
      case ValueType::Ptr:
        return Value(&ptr);
      case ValueType::Object:
        return Value(obj->get_pointer());
      case ValueType::Array:
        return Value(arr->get_pointer());
      case ValueType::Function:
        return Value(func);
      default:
        Value::error(Error::BadOperand);
    }
  }

  Value Value::op_read() const
  {
    if (tag == ValueType::Cown)
    {
      Value r = (*this).copy_value();
      r.readonly = true;
      return r;
    }

    Value::error(Error::BadOperand);
  }

  template<bool is_register>
  void Value::inc() const
  {
    switch (tag)
    {
      case ValueType::Object:
      case ValueType::FieldRef:
        if (!readonly)
          obj->template inc<is_register>();
        break;

      case ValueType::Array:
      case ValueType::ArrayRef:
        if (!readonly)
          arr->inc<is_register>();
        break;

      case ValueType::Cown:
      case ValueType::CownRef:
        // Cowns do not have a stack RC, so doesn't mater if it is
        // a stack RC or not.
        cown->inc();
        break;

      default:
        break;
    }
  }

  template<bool is_register>
  void Value::dec() const
  {
    switch (tag)
    {
      case ValueType::Object:
      case ValueType::FieldRef:
        if (!readonly)
          obj->dec<is_register>();
        break;

      case ValueType::Array:
      case ValueType::ArrayRef:
        if (!readonly)
          arr->dec<is_register>();
        break;

      case ValueType::Cown:
      case ValueType::CownRef:
        // Cowns do not have a stack RC, so doesn't mater if it is
        // a stack RC or not.
        cown->dec();
        break;

      default:
        break;
    }
  }

  Location Value::location() const
  {
    switch (tag)
    {
      case ValueType::RegisterRef:
        return idx;

      case ValueType::Object:
      case ValueType::FieldRef:
        return obj->location();

      case ValueType::Array:
      case ValueType::ArrayRef:
        return arr->location();

      case ValueType::Cown:
      case ValueType::CownRef:
        return loc::Immutable;

      default:
        return loc::Immortal;
    }
  }

  Region* Value::region() const
  {
    switch (tag)
    {
      case ValueType::Object:
      case ValueType::FieldRef:
      {
        auto r = obj->region();
        if (r)
          return r;
        break;
      }

      case ValueType::Array:
      case ValueType::ArrayRef:
      {
        auto r = arr->region();
        if (r)
          return r;
        break;
      }

      default:
        break;
    }

    Value::error(Error::BadAllocTarget);
  }

  void Value::immortalize()
  {
    LOG(Trace) << "Immortalizing value of type " << int(tag) << "@" << this;
    switch (tag)
    {
      case ValueType::Object:
      case ValueType::FieldRef:
        obj->immortalize();
        break;

      case ValueType::Array:
      case ValueType::ArrayRef:
        arr->immortalize();
        break;

      default:
        break;
    }
  }

  void Value::drop_reg()
  {
    dec<true>();
    tag = ValueType::Invalid;
  }

  void Value::field_drop()
  {
    dec<false>();
    tag = ValueType::Invalid;
  }

  Register Value::ref(bool move, size_t field)
  {
    switch (tag)
    {
      case ValueType::Object:
      {
        auto f = obj->field(field);

        if (move)
          tag = ValueType::Invalid;
        else
          // Moving into a register requires an increment
          // on stack rc, as well as the object.
          inc<true>();

        return Register(Value(obj, f, readonly));
      }

      case ValueType::Cown:
      {
        if (move)
          tag = ValueType::Invalid;
        else
          // Cowns are always unregioned, so can skip the stack rc checks.
          inc<false>();

        // Cowns don't need a stack rc, so can be freely lifted to
        // registers.
        return Register(Value(cown, false));
      }

      default:
        Value::error(Error::BadRefTarget);
    }
  }

  Register Value::arrayref(bool move, size_t i) const
  {
    if (tag != ValueType::Array)
      Value::error(Error::BadRefTarget);

    if (i >= arr->get_size())
      Value::error(Error::BadArrayIndex);

    if (move)
      // Const can be ignored here as only required when move is false.
      const_cast<Value*>(this)->tag = ValueType::Invalid;
    else
      // Moving into a register requires an increment
      // on stack rc, as well as the object.
      inc<true>();

    return Register(Value(arr, i, readonly));
  }

  Register Value::load() const
  {
    Value v;

    switch (tag)
    {
      case ValueType::RegisterRef:
        return (*reg).copy_reg();

      case ValueType::FieldRef:
        v = obj->load(idx);
        break;

      case ValueType::ArrayRef:
        v = arr->load(idx);
        break;

      case ValueType::CownRef:
      {
        Register r{cown->load()};
        r.readonly = readonly;
        return r;
      }
      default:
        Value::error(Error::BadLoadTarget);
    }

    v.inc<true>();
    v.readonly = readonly;
    return Register(std::move(v));
  }

  template<bool is_move>
  Register Value::store(Reg<is_move> v) const
  {
    if (readonly)
      Value::error(Error::BadStoreTarget);

    // Currently only cowns provide read-only access.  That means it
    // is never valid to store a read-only reference any where.
    if (v.readonly)
      Value::error(Error::BadStore);

    switch (tag)
    {
      case ValueType::RegisterRef:
      {
        auto vloc = v.location();

        if (loc::is_stack(vloc) && (vloc > idx))
          Value::error(Error::BadStoreTarget);

        // Should also check for frame local?
        if (loc::is_region(vloc) && loc::to_region(vloc)->is_frame_local())
        {
          auto vr = loc::to_region(vloc);
          if (vr->get_parent() > idx)
            // TODO This should perform a drag rather than failing.
            // We need to move the frame local region to the frame local
            // region associated with the register ref.
            Value::error(Error::BadStoreTarget);
        }

        Register prev = std::move(*reg);

        if constexpr (is_move)
          *reg = std::move(v);
        else
          *reg = v.copy_reg();

        return prev;
      }

      case ValueType::FieldRef:
        return obj->store<is_move>(idx, std::forward<Reg<is_move>>(v));

      case ValueType::ArrayRef:
        return arr->store<is_move>(idx, std::forward<Reg<is_move>>(v));

      case ValueType::CownRef:
        return cown->store<is_move>(std::forward<Reg<is_move>>(v));

      default:
        Value::error(Error::BadStoreTarget);
    }
  }

  // Create instances of templated store
  template Register Value::store<true>(Reg<true> v) const;
  template Register Value::store<false>(Reg<false> v) const;

  Function* Value::method(size_t w) const
  {
    return Program::get().cls(type_id()).method(w);
  }

  Value Value::convert(ValueType to) const
  {
    if ((tag > ValueType::Function) || (to > ValueType::F64))
      Value::error(Error::BadConversion);

    if (tag == to)
      return copy_value();

    if (tag < ValueType::F32)
      return Value(to, get<uint64_t>());

    return Value(to, get<double>());
  }

  std::string Value::to_string() const
  {
    switch (tag)
    {
      case ValueType::None:
        return "none";

      case ValueType::Bool:
        return b ? "true" : "false";

      case ValueType::I8:
        return std::to_string(i8);

      case ValueType::I16:
        return std::to_string(i16);

      case ValueType::I32:
        return std::to_string(i32);

      case ValueType::I64:
        return std::to_string(i64);

      case ValueType::U8:
        return std::to_string(u8);

      case ValueType::U16:
        return std::to_string(u16);

      case ValueType::U32:
        return std::to_string(u32);

      case ValueType::U64:
        return std::to_string(u64);

      case ValueType::ILong:
        return std::to_string(ilong);

      case ValueType::ULong:
        return std::to_string(ulong);

      case ValueType::ISize:
        return std::to_string(isize);

      case ValueType::USize:
        return std::to_string(usize);

      case ValueType::F32:
        return std::to_string(f32);

      case ValueType::F64:
        return std::to_string(f64);

      case ValueType::Ptr:
        return std::format("ptr {}", ptr);

      case ValueType::Object:
        return obj->to_string();

      case ValueType::Array:
        return arr->to_string();

      case ValueType::Cown:
        return cown->to_string();

      case ValueType::RegisterRef:
        return std::format("ref {}", reg->to_string());

      case ValueType::FieldRef:
        return std::format(
          "ref [{}] {}",
          Program::get().di_field(obj->cls(), idx),
          obj->to_string());

      case ValueType::ArrayRef:
      {
        size_t i = idx;
        return std::format("ref [{}] {}", i, arr->to_string());
      }

      case ValueType::CownRef:
        return std::format("ref {}", cown->to_string());

      case ValueType::Function:
        return Program::get().di_function(func);

      case ValueType::Error:
      {
        auto di =
          Program::get().debug_info(reinterpret_cast<Function*>(err.func), idx);
        return std::format("{}\n{}", errormsg(err.error), di);
      }

      case ValueType::Invalid:
        return "invalid";

      default:
        // Unreachable.
        assert(false);
        return "unknown";
    }
  }
}
