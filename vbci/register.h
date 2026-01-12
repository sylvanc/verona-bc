#pragma once

/**
 *  Implementation of a register in the virtual machine.
 *
 * This holds a value and manages the ownership semantics of that value.
 */
#include "value.h"

namespace vbci
{
  struct ValueTransfer : Value
  {
  public:
    using Value::Value;

    ValueTransfer(const Value& v) : Value(v) {}
  };

  struct ValueBorrow : Value
  {
  public:
    using Value::Value;

    ValueBorrow(const Value& v) : Value(v) {}
  };

  struct ValueImmortal : Value
  {
  public:
    using Value::Value;

    ValueImmortal(const Value& v) : Value(v) {}
  };

  struct Register : private Value
  {
  private:
    // This sets the internal value without modifying reference counts.
    // Use with caution.
    // The caller is expected to provide an RC and a stack RC for this value.
    void set_raw_unsafe(const Value& v)
    {
      Value::operator=(v);
    }

  public:
    Register() = default;

    Register(Register&& v)
    {
      set_raw_unsafe(v);
      v.clear_unsafe();
    };

    Register(ValueBorrow v)
    {
      set_raw_unsafe(v);
      v.inc<true>();
    };

    Register(ValueImmortal v)
    {
      assert(v.location() == Location::immortal());
      set_raw_unsafe(v);
    };

    Register(ValueTransfer v)
    {
      set_raw_unsafe(v);
    };

    ~Register()
    {
      clear();
    }

    const Value* operator->() const
    {
      return static_cast<const Value*>(this);
    }

    ValueTransfer extract()
    {
      ValueTransfer v = *this;
      clear_unsafe();
      return v;
    }

    operator ValueBorrow() const
    {
      return ValueBorrow(static_cast<const Value&>(*this));
    }

    ValueBorrow borrow() const
    {
      return *this;
    }

    template<typename F>
    void replace_unsafe(F fun)
    {
      Value old = *this;
      Value new_value = fun();
      set_raw_unsafe(new_value);
      old.dec<true>();
    }

    void operator=(ValueTransfer v)
    {
      replace_unsafe([&]() { return v; });
    }

    void operator=(ValueBorrow v)
    {
      replace_unsafe([&]() {
        v.inc<true>();
        return v;
      });
    }

    void operator=(ValueImmortal v)
    {
      replace_unsafe([&]() { return v; });
    }

    void clear_unsafe()
    {
      set_raw_unsafe(Value());
    }

    void operator=(Register&& r)
    {
      if (this == &r)
        return;

      replace_unsafe([&]() -> Value {
        Value v = r;
        r.clear_unsafe();
        return v;
      });
    }

    void operator=(const Register& v)
    {
      replace_unsafe([&]() -> Value {
        v.inc<true>();
        return v;
      });
    }

    void clear()
    {
      replace_unsafe([&]() { return Value(); });
    }

    void from_load(const Register& src)
    {
      replace_unsafe([&]() {
        Value v = src.load_reference();
        v.inc<true>();
        return v;
      });
    }

    template<bool is_move>
    void from_exchange(const Register& reference, Reg<is_move> new_value)
    {
      reference.exchange<is_move>(*this, std::forward<Reg<is_move>>(new_value));
    }

    template<bool is_move>
    void from_field_ref(Reg<is_move> src, size_t field_id)
    {
      switch (src.get_value_type())
      {
        case ValueType::Object:
        {
          auto obj = src.get_object();
          auto f = obj->field(field_id);

          auto readonly = src.is_readonly();

          replace_unsafe([&]() {
            if constexpr (is_move)
              src.clear_unsafe();
            else
              src.template inc<true>();
            return Value(obj, f, readonly);
          });
          return;
        }
        case ValueType::Cown:
        {
          auto cown = src.get_cown();
          snmalloc::UNUSED(field_id);

          auto readonly = src.is_readonly();

          replace_unsafe([&]() {
            if constexpr (is_move)
              src.clear_unsafe();
            else
              src.template inc<false>();
            return Value(cown, readonly);
          });
          return;
        }
        default:
          Value::error(Error::BadRefTarget);
      }
    }

    template<bool is_move>
    void from_array_ref(Reg<is_move> src, size_t index)
    {
      if (src.get_value_type() != ValueType::Array)
        Value::error(Error::BadRefTarget);

      auto arr = src.get_array();

      if (index >= arr->get_size())
        Value::error(Error::BadArrayIndex);

      auto readonly = src.is_readonly();

      if constexpr (is_move)
        src.clear_unsafe();
      else
        src.template inc<true>();

      replace_unsafe([&]() { return Value(arr, index, readonly); });
    }
  };

  // Add pretty printing for registers.
  inline std::ostream& operator<<(std::ostream& os, const Register& r)
  {
    return os << static_cast<const Value&>(r.borrow());
  }
} // namespace vbci