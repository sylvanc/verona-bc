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
        obj->inc();
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
        obj->dec();
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

  Value Value::makeref(Program* program, FieldId field)
  {
    switch (tag)
    {
      case ValueType::Object:
      {
        auto& cls = program->classes.at(obj->class_id);
        auto find = cls.fields.find(field);
        if (find == cls.fields.end())
          throw Value(Error::BadField);

        idx = find->second;
        tag = ValueType::Ref;
        break;
      }

      case ValueType::Cown:
      {
        tag = ValueType::CownRef;
        break;
      }

      default:
        throw Value(Error::BadRefTarget);
    }

    return *this;
  }

  Value Value::load()
  {
    Value v;

    switch (tag)
    {
      case ValueType::Ref:
        v = obj->fields[idx];
        break;

      case ValueType::CownRef:
        v = cown->content;
        break;

      default:
        throw Value(Error::BadLoadTarget);
    }

    v.inc();
    return v;
  }

  Value Value::store(Value& v)
  {
    switch (tag)
    {
      case ValueType::Ref:
        return obj->store(idx, v);

      case ValueType::CownRef:
        return cown->store(v);

      default:
        throw Value(Error::BadStoreTarget);
    }
  }

  Function* Value::method(Program* program, FuncId w)
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

      case ValueType::Cown:
      case ValueType::CownRef:
        return Immutable;

      default:
        return Immortal;
    }
  }
}
