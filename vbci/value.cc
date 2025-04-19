#include "array.h"
#include "cown.h"
#include "object.h"
#include "program.h"

namespace vbci
{
  void Value::inc()
  {
    switch (tag)
    {
      case ValueType::Object:
      case ValueType::Ref:
        if (!readonly)
          obj->inc();
        break;

      case ValueType::Array:
      case ValueType::ArrayRef:
        if (!readonly)
          arr->inc();
        break;

      case ValueType::Cown:
      case ValueType::CownRef:
        cown->inc();
        break;

      default:
        break;
    }
  }

  void Value::dec()
  {
    switch (tag)
    {
      case ValueType::Object:
      case ValueType::Ref:
        if (!readonly)
          obj->dec();
        break;

      case ValueType::Array:
      case ValueType::ArrayRef:
        if (!readonly)
          arr->dec();
        break;

      case ValueType::Cown:
      case ValueType::CownRef:
        cown->dec();
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

  Value Value::makeref(Program* program, ArgType arg_type, Id field)
  {
    switch (tag)
    {
      case ValueType::Object:
      {
        auto& cls = program->classes.at(obj->class_id);
        auto find = cls.fields.find(field);
        if (find == cls.fields.end())
          throw Value(Error::BadField);

        if (arg_type == ArgType::Move)
          tag = ValueType::Invalid;
        else
          inc();

        return Value(obj, find->second, readonly);
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

  Value Value::makearrayref(ArgType arg_type, size_t i)
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
        v = obj->fields[idx];
        break;

      case ValueType::ArrayRef:
        v = arr->data[idx];
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

  Function* Value::method(Program* program, Id w)
  {
    if (tag == ValueType::Object)
      return program->classes.at(obj->class_id).method(w);

    auto type_id = static_cast<size_t>(tag);

    if (type_id >= program->primitives.size())
      throw Value(Error::BadMethodTarget);

    return program->primitives.at(type_id).method(w);
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

  Location Value::location()
  {
    switch (tag)
    {
      case ValueType::Object:
      case ValueType::Ref:
        return obj->loc;

      case ValueType::Array:
      case ValueType::ArrayRef:
        return arr->loc;

      case ValueType::Cown:
      case ValueType::CownRef:
        return Immutable;

      default:
        return Immortal;
    }
  }
}
