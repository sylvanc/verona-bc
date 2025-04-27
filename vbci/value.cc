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

  Value::Value(Object* obj) : obj(obj), tag(ValueType::Object), readonly(0) {}

  Value::Value(Object* obj, bool ro)
  : obj(obj), tag(ValueType::Object), readonly(ro)
  {}

  Value::Value(Array* arr) : arr(arr), tag(ValueType::Array), readonly(0) {}

  Value::Value(Cown* cown) : cown(cown), tag(ValueType::Cown) {}

  Value::Value(Object* obj, FieldIdx f, bool ro)
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

  Value Value::from_ffi(Id type_id, uint64_t v)
  {
    Value value(type::val(type_id));
    value.u64 = v;
    return value;
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
        return type::cown(cown->type_id);

      case ValueType::Ref:
        return type::ref(obj->field_type_id(idx));

      case ValueType::ArrayRef:
        return type::ref(arr->content_type_id());

      case ValueType::CownRef:
        return type::ref(cown->type_id);

      case ValueType::Function:
        // TODO: allow function types?
        return type::dyn();

      case ValueType::Error:
      case ValueType::Invalid:
        return type::dyn();

      default:
        return type::val(tag);
    }
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

  size_t Value::to_index()
  {
    switch (tag)
    {
      case ValueType::U8:
        return get<uint8_t>();

      case ValueType::U16:
        return get<uint16_t>();

      case ValueType::U32:
        return get<uint32_t>();

      case ValueType::U64:
        return get<uint64_t>();

      default:
        throw Value(Error::BadRefTarget);
    }
  }

  uint64_t Value::to_ffi()
  {
    return u64;
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

  Value Value::swap(ArgType arg_type, bool stack, Value& that)
  {
    // This is called from Cown::store and Header::base_store.
    // Safety is already checked.
    assert(this != &that);
    auto prev = std::move(*this);
    std::memcpy(this, &that, sizeof(Value));

    // If the store location is a stack alloc, we need to increment the
    // region stack RC as well as the allocation RC.
    if (arg_type == ArgType::Copy)
      inc(stack);
    else
      that.tag = ValueType::Invalid;

    return prev;
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

  Value Value::ref(ArgType arg_type, Id field)
  {
    switch (tag)
    {
      case ValueType::Object:
      {
        auto f = obj->field(field);

        if (arg_type == ArgType::Move)
          tag = ValueType::Invalid;
        else
          inc();

        return Value(obj, f, readonly);
      }

      case ValueType::Cown:
      {
        if (arg_type == ArgType::Move)
          tag = ValueType::Invalid;
        else
          inc();

        return Value(cown, false);
      }

      default:
        throw Value(Error::BadRefTarget);
    }
  }

  Value Value::arrayref(ArgType arg_type, size_t i)
  {
    if (tag != ValueType::Array)
      throw Value(Error::BadRefTarget);

    if (arg_type == ArgType::Move)
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
        v = cown->content;
        break;

      default:
        throw Value(Error::BadLoadTarget);
    }

    v.inc();
    v.readonly = readonly;
    return v;
  }

  Value Value::store(ArgType arg_type, Value& v)
  {
    if (readonly)
      throw Value(Error::BadStoreTarget);

    if (v.readonly)
      throw Value(Error::BadStore);

    switch (tag)
    {
      case ValueType::Ref:
        return obj->store(arg_type, idx, v);

      case ValueType::ArrayRef:
        return arr->store(arg_type, idx, v);

      case ValueType::CownRef:
        return cown->store(arg_type, v);

      default:
        throw Value(Error::BadStoreTarget);
    }
  }

  Function* Value::method(Id w)
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

  Function* Value::function()
  {
    if (tag != ValueType::Function)
      throw Value(Error::UnknownFunction);

    return func;
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
        return std::to_string(b);

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
      case ValueType::ULong:
      case ValueType::ISize:
      case ValueType::USize:
      case ValueType::Ptr:
        return "unreachable";

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
        FieldIdx f = idx;
        return std::format("ref [{}] {}", f, arr->to_string());
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
