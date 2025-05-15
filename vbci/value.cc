#include "array.h"
#include "cown.h"
#include "object.h"
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
  Value::Value(float f32) : f32(f32), tag(ValueType::F32) {}
  Value::Value(double f64) : f64(f64), tag(ValueType::F64) {}
  Value::Value(void* ptr) : ptr(ptr), tag(ValueType::Ptr) {}
  Value::Value(Object* obj) : obj(obj), tag(ValueType::Object), readonly(0) {}

  Value::Value(Object* obj, bool ro)
  : obj(obj), tag(ValueType::Object), readonly(ro)
  {}

  Value::Value(Array* arr) : arr(arr), tag(ValueType::Array), readonly(0) {}
  Value::Value(Cown* cown) : cown(cown), tag(ValueType::Cown) {}

  Value::Value(Object* obj, size_t f, bool ro)
  : obj(obj), idx(f), tag(ValueType::Ref), readonly(ro)
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
  }

  Value::Value(Function* func) : func(func), tag(ValueType::Function)
  {
    if (!func)
      throw Value(Error::MethodNotFound);
  }

  Value::Value(const Value& that)
  {
    std::memcpy(this, &that, sizeof(Value));
    inc();
  }

  Value::Value(Value&& that) noexcept
  {
    std::memcpy(this, &that, sizeof(Value));
    that.tag = ValueType::Invalid;
  }

  Value& Value::operator=(const Value& that)
  {
    if (this == &that)
      return *this;

    dec();
    std::memcpy(this, &that, sizeof(Value));
    inc();
    return *this;
  }

  Value& Value::operator=(Value&& that) noexcept
  {
    if (this == &that)
      return *this;

    dec();
    std::memcpy(this, &that, sizeof(Value));
    that.tag = ValueType::Invalid;
    return *this;
  }

  Value Value::none()
  {
    return Value(ValueType::None);
  }

  Value Value::null()
  {
    return static_cast<void*>(nullptr);
  }

  ValueType Value::type()
  {
    return tag;
  }

  Value Value::from_ffi(ValueType t, uint64_t v)
  {
    Value value(t);
    value.u64 = v;
    return value;
  }

  Value Value::from_addr(ValueType t, void* v)
  {
    Value value(t);

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
        std::memcpy(&value, v, sizeof(Value));
        break;
    }

    return value;
  }

  void Value::to_addr(bool move, void* v)
  {
    switch (tag)
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

    if (move)
      tag = ValueType::Invalid;
    else
      inc();
  }

  Id Value::type_id()
  {
    switch (tag)
    {
      case ValueType::Object:
        return obj->type_id();

      case ValueType::Array:
        return arr->array_type_id();

      case ValueType::Cown:
        return cown->cown_type_id();

      case ValueType::Ref:
        return type::ref(obj->field_type_id(idx));

      case ValueType::ArrayRef:
        return type::ref(arr->content_type_id());

      case ValueType::CownRef:
        return type::ref(cown->content_type_id());

      case ValueType::Function:
        return type::dyn();

      case ValueType::Error:
      case ValueType::Invalid:
        return type::dyn();

      default:
        return type::val(tag);
    }
  }

  bool Value::is_readonly()
  {
    return readonly;
  }

  bool Value::is_function()
  {
    return tag == ValueType::Function;
  }

  bool Value::is_sendable()
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
      case ValueType::Ref:
      case ValueType::ArrayRef:
      case ValueType::CownRef:
        return false;

      default:
        return true;
    }
  }

  bool Value::is_error()
  {
    return tag == ValueType::Error;
  }

  bool Value::get_bool()
  {
    if (tag != ValueType::Bool)
      throw Value(Error::BadConversion);

    return b;
  }

  int32_t Value::get_i32()
  {
    if (tag != ValueType::I32)
      throw Value(Error::BadConversion);

    return i32;
  }

  Cown* Value::get_cown()
  {
    if (tag != ValueType::Cown)
      throw Value(Error::BadConversion);

    return cown;
  }

  Object* Value::get_object()
  {
    if (tag != ValueType::Object)
      throw Value(Error::BadConversion);

    return obj;
  }

  Function* Value::function()
  {
    if (tag != ValueType::Function)
      throw Value(Error::UnknownFunction);

    return func;
  }

  size_t Value::to_index()
  {
    switch (tag)
    {
      case ValueType::U8:
      case ValueType::U16:
      case ValueType::U32:
      case ValueType::U64:
      case ValueType::ULong:
      case ValueType::USize:
        return get<size_t>();

      default:
        throw Value(Error::BadRefTarget);
    }
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

  Value Value::op_len()
  {
    if (tag == ValueType::Array)
      return Value(ValueType::USize, arr->get_size());

    throw Value(Error::BadOperand);
  }

  Value Value::op_ptr()
  {
    switch (tag)
    {
      case ValueType::None:
        return static_cast<void*>(nullptr);
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
        return obj->get_pointer();
      case ValueType::Array:
        return arr->get_pointer();
      case ValueType::Function:
        return func;
      default:
        throw Value(Error::BadOperand);
    }
  }

  Value Value::op_read()
  {
    if (tag == ValueType::Cown)
    {
      Value r = *this;
      r.readonly = true;
      return r;
    }

    throw Value(Error::BadOperand);
  }

  void Value::inc(bool reg)
  {
    switch (tag)
    {
      case ValueType::Object:
      case ValueType::Ref:
        if (!readonly)
          obj->inc(reg);
        break;

      case ValueType::Array:
      case ValueType::ArrayRef:
        if (!readonly)
          arr->inc(reg);
        break;

      case ValueType::Cown:
      case ValueType::CownRef:
        cown->inc();
        break;

      default:
        break;
    }
  }

  void Value::dec(bool reg)
  {
    switch (tag)
    {
      case ValueType::Object:
      case ValueType::Ref:
        if (!readonly)
          obj->dec(reg);
        break;

      case ValueType::Array:
      case ValueType::ArrayRef:
        if (!readonly)
          arr->dec(reg);
        break;

      case ValueType::Cown:
      case ValueType::CownRef:
        cown->dec();
        break;

      default:
        break;
    }
  }

  Location Value::location()
  {
    switch (tag)
    {
      case ValueType::Object:
      case ValueType::Ref:
        return obj->location();

      case ValueType::Array:
      case ValueType::ArrayRef:
        return arr->location();

      case ValueType::Cown:
      case ValueType::CownRef:
        return Immutable;

      default:
        return Immortal;
    }
  }

  Region* Value::region()
  {
    switch (tag)
    {
      case ValueType::Object:
      case ValueType::Ref:
        return obj->region();

      case ValueType::Array:
      case ValueType::ArrayRef:
        return arr->region();

      default:
        throw Value(Error::BadAllocTarget);
    }
  }

  void Value::immortalize()
  {
    switch (tag)
    {
      case ValueType::Object:
      case ValueType::Ref:
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

  void Value::drop()
  {
    dec();
    tag = ValueType::Invalid;
  }

  void Value::field_drop()
  {
    dec(false);
    tag = ValueType::Invalid;
  }

  Value Value::ref(bool move, size_t field)
  {
    switch (tag)
    {
      case ValueType::Object:
      {
        auto f = obj->field(field);

        if (move)
          tag = ValueType::Invalid;
        else
          inc();

        return Value(obj, f, readonly);
      }

      case ValueType::Cown:
      {
        if (move)
          tag = ValueType::Invalid;
        else
          inc();

        return Value(cown, false);
      }

      default:
        throw Value(Error::BadRefTarget);
    }
  }

  Value Value::arrayref(bool move, size_t i)
  {
    if (tag != ValueType::Array)
      throw Value(Error::BadRefTarget);

    if (i >= arr->get_size())
      throw Value(Error::BadArrayIndex);

    if (move)
      tag = ValueType::Invalid;
    else
      inc();

    return Value(arr, i, readonly);
  }

  Value Value::load()
  {
    Value v;

    switch (tag)
    {
      case ValueType::Ref:
        v = obj->load(idx);
        break;

      case ValueType::ArrayRef:
        v = arr->load(idx);
        break;

      case ValueType::CownRef:
        v = cown->load();
        break;

      default:
        throw Value(Error::BadLoadTarget);
    }

    v.inc();
    v.readonly = readonly;
    return v;
  }

  Value Value::store(bool move, Value& v)
  {
    if (readonly)
      throw Value(Error::BadStoreTarget);

    if (v.readonly)
      throw Value(Error::BadStore);

    switch (tag)
    {
      case ValueType::Ref:
        return obj->store(move, idx, v);

      case ValueType::ArrayRef:
        return arr->store(move, idx, v);

      case ValueType::CownRef:
        return cown->store(move, v);

      default:
        throw Value(Error::BadStoreTarget);
    }
  }

  Function* Value::method(size_t w)
  {
    if (tag == ValueType::Object)
      return obj->method(w);

    if (+tag > +ValueType::F64)
      throw Value(Error::BadMethodTarget);

    return Program::get().primitive(+tag).method(w);
  }

  Value Value::convert(ValueType to)
  {
    if (
      (tag < ValueType::I8) || (tag > ValueType::F64) || (to < ValueType::I8) ||
      (to > ValueType::F64))
      throw Value(Error::BadConversion);

    if (tag == to)
      return *this;

    Value v(to);

    if (tag < ValueType::F32)
      v.set(get<uint64_t>());
    else
      v.set(get<double>());

    return v;
  }

  void Value::annotate(Function* func, PC pc)
  {
    assert(tag == ValueType::Error);
    err.func = uintptr_t(func);
    idx = pc;
  }

  std::string Value::to_string()
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

      case ValueType::F32:
        return std::to_string(f32);

      case ValueType::F64:
        return std::to_string(f64);

      case ValueType::ILong:
        return std::to_string(ilong);

      case ValueType::ULong:
        return std::to_string(ulong);

      case ValueType::ISize:
        return std::to_string(isize);

      case ValueType::USize:
        return std::to_string(usize);

      case ValueType::Ptr:
        return std::format("ptr {}", ptr);

      case ValueType::Object:
        return obj->to_string();

      case ValueType::Array:
        return arr->to_string();

      case ValueType::Cown:
        return cown->to_string();

      case ValueType::Ref:
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
    }
  }
}
