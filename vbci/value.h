#pragma once

#include "ident.h"

#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <numbers>
#include <vbci.h>

namespace vbci
{
  // A value is 16 bytes.
  struct Value
  {
  private:
    struct Err
    {
      uintptr_t func : 56;
      Error error : 8;
    };

    union
    {
      bool b;
      int8_t i8;
      int16_t i16;
      int32_t i32;
      int64_t i64;
      uint8_t u8;
      uint16_t u16;
      uint32_t u32;
      uint64_t u64;
      long ilong;
      unsigned long ulong;
      ssize_t isize;
      size_t usize;
      float f32;
      double f64;
      void* ptr;
      Object* obj;
      Array* arr;
      Cown* cown;
      Err err;
      Function* func;
    };

    uint64_t idx : 56;
    ValueType tag : 7;
    uint8_t readonly : 1;

    Value(ValueType t) : tag(t) {}

  public:
    Value();
    Value(bool b);
    Value(uint8_t u8);
    Value(uint16_t u16);
    Value(uint32_t u32);
    Value(uint64_t u64);
    Value(int8_t i8);
    Value(int16_t i16);
    Value(int32_t i32);
    Value(int64_t i64);
    Value(float f32);
    Value(double f64);
    Value(Object* obj);
    Value(Object* obj, bool ro);
    Value(Array* arr);
    Value(Cown* cown);
    Value(Object* obj, FieldIdx f, bool ro);
    Value(Array* arr, size_t idx, bool ro);
    Value(Cown* cown, bool ro);
    Value(Error error);
    Value(Function* func);
    Value(const Value& that);
    Value(Value&& that) noexcept;
    Value& operator=(const Value& that);
    Value& operator=(Value&& that) noexcept;

    template<typename T>
    Value(ValueType t, T v) : tag(t)
    {
      set<T>(v);
    }

    static Value none();
    static Value from_ffi(ValueType t, uint64_t v);
    static Value from_addr(ValueType t, void* v);
    void to_addr(ArgType arg_type, void* v);

    ValueType type();
    Id type_id();

    bool is_error();
    bool get_bool();
    int32_t get_i32();
    size_t to_index();
    void* address_of();

    Location location();
    Region* region();

    void drop();
    void field_drop();
    Value ref(ArgType arg_type, Id field);
    Value arrayref(ArgType arg_type, size_t i);
    Value load();
    Value store(ArgType arg_type, Value& v);
    Function* method(Id w);
    Value convert(ValueType to);
    Function* function();

    void annotate(Function* func, PC pc);
    std::string to_string();

#define make_unop(name, func) \
  struct name \
  { \
    template<typename T> \
    constexpr auto operator()(T&& arg) const -> decltype(func) \
    { \
      return func; \
    } \
  }

#define make_binop(name, func) \
  struct name \
  { \
    template<typename T, typename U> \
    constexpr auto operator()(T&& lhs, U&& rhs) const -> decltype(func) \
    { \
      return func; \
    } \
  }

    Value op_add(Value& v)
    {
      return binop<nobinop, std::plus<>>(v);
    }

    Value op_sub(Value& v)
    {
      return binop<nobinop, std::minus<>>(v);
    }

    Value op_mul(Value& v)
    {
      return binop<nobinop, std::multiplies<>>(v);
    }

    Value op_div(Value& v)
    {
      return binop<nobinop, std::divides<>>(v);
    }

    make_binop(fmod, std::fmod(lhs, rhs));
    Value op_mod(Value& v)
    {
      return binop<nobinop, std::modulus<>, std::modulus<>, fmod>(v);
    }

    make_binop(fpow, std::pow(lhs, rhs));
    Value op_pow(Value& v)
    {
      return binop<nobinop, nobinop, nobinop, fpow>(v);
    }

    Value op_and(Value& v)
    {
      return binop<std::logical_and<>, std::bit_and<>, std::bit_and<>, nobinop>(
        v);
    }

    Value op_or(Value& v)
    {
      return binop<std::logical_or<>, std::bit_or<>, std::bit_or<>, nobinop>(v);
    }

    Value op_xor(Value& v)
    {
      return binop<std::bit_xor<>, std::bit_xor<>, std::bit_xor<>, nobinop>(v);
    }

    make_binop(bit_left_shift, lhs << rhs);
    Value op_shl(Value& v)
    {
      return binop<nobinop, bit_left_shift, bit_left_shift, nobinop>(v);
    }

    make_binop(bit_right_shift, lhs >> rhs);
    Value op_shr(Value& v)
    {
      return binop<nobinop, bit_right_shift, bit_right_shift, nobinop>(v);
    }

    Value op_eq(Value& v)
    {
      return binop<std::equal_to<>>(v);
    }

    Value op_ne(Value& v)
    {
      return binop<std::not_equal_to<>>(v);
    }

    Value op_lt(Value& v)
    {
      return binop<std::less<>>(v);
    }

    Value op_le(Value& v)
    {
      return binop<std::less_equal<>>(v);
    }

    Value op_gt(Value& v)
    {
      return binop<std::greater<>>(v);
    }

    Value op_ge(Value& v)
    {
      return binop<std::greater_equal<>>(v);
    }

    make_binop(min, std::min(lhs, rhs));
    Value op_min(Value& v)
    {
      return binop<min>(v);
    }

    make_binop(max, std::max(lhs, rhs));
    Value op_max(Value& v)
    {
      return binop<max>(v);
    }

    make_binop(logbase, std::log(lhs) / std::log(rhs));
    Value op_logbase(Value& v)
    {
      return binop<nobinop, nobinop, nobinop, logbase>(v);
    }

    make_binop(atan2, std::atan2(lhs, rhs));
    Value op_atan2(Value& v)
    {
      return binop<nobinop, nobinop, nobinop, atan2>(v);
    }

    Value op_neg()
    {
      return unop<nounop, std::negate<>>();
    }

    Value op_not()
    {
      return unop<std::logical_not<>, std::bit_not<>, std::bit_not<>, nounop>();
    }

    make_unop(abs, std::abs(arg));
    Value op_abs()
    {
      return unop<nounop, abs, nounop, abs>();
    }

    make_unop(ceil, std::ceil(arg));
    Value op_ceil()
    {
      return unop<nounop, nounop, nounop, ceil>();
    }

    make_unop(floor, std::floor(arg));
    Value op_floor()
    {
      return unop<nounop, nounop, nounop, floor>();
    }

    make_unop(exp, std::exp(arg));
    Value op_exp()
    {
      return unop<nounop, nounop, nounop, exp>();
    }

    make_unop(log, std::log(arg));
    Value op_log()
    {
      return unop<nounop, nounop, nounop, log>();
    }

    make_unop(sqrt, std::sqrt(arg));
    Value op_sqrt()
    {
      return unop<nounop, nounop, nounop, sqrt>();
    }

    make_unop(cbrt, std::cbrt(arg));
    Value op_cbrt()
    {
      return unop<nounop, nounop, nounop, cbrt>();
    }

    make_unop(isinf, std::isinf(arg));
    Value op_isinf()
    {
      return unop<nounop, nounop, nounop, isinf>();
    }

    make_unop(isnan, std::isnan(arg));
    Value op_isnan()
    {
      return unop<nounop, nounop, nounop, isnan>();
    }

    make_unop(sin, std::sin(arg));
    Value op_sin()
    {
      return unop<nounop, nounop, nounop, sin>();
    }

    make_unop(cos, std::cos(arg));
    Value op_cos()
    {
      return unop<nounop, nounop, nounop, cos>();
    }

    make_unop(tan, std::tan(arg));
    Value op_tan()
    {
      return unop<nounop, nounop, nounop, tan>();
    }

    make_unop(asin, std::asin(arg));
    Value op_asin()
    {
      return unop<nounop, nounop, nounop, asin>();
    }

    make_unop(acos, std::acos(arg));
    Value op_acos()
    {
      return unop<nounop, nounop, nounop, acos>();
    }

    make_unop(atan, std::atan(arg));
    Value op_atan()
    {
      return unop<nounop, nounop, nounop, atan>();
    }

    make_unop(sinh, std::sinh(arg));
    Value op_sinh()
    {
      return unop<nounop, nounop, nounop, sinh>();
    }

    make_unop(cosh, std::cosh(arg));
    Value op_cosh()
    {
      return unop<nounop, nounop, nounop, cosh>();
    }

    make_unop(tanh, std::tanh(arg));
    Value op_tanh()
    {
      return unop<nounop, nounop, nounop, tanh>();
    }

    make_unop(asinh, std::asinh(arg));
    Value op_asinh()
    {
      return unop<nounop, nounop, nounop, asinh>();
    }

    make_unop(acosh, std::acosh(arg));
    Value op_acosh()
    {
      return unop<nounop, nounop, nounop, acosh>();
    }

    make_unop(atanh, std::atanh(arg));
    Value op_atanh()
    {
      return unop<nounop, nounop, nounop, atanh>();
    }

    static Value e()
    {
      return Value(std::numbers::e);
    }

    static Value pi()
    {
      return Value(std::numbers::pi);
    }

    static Value inf()
    {
      return Value(std::numeric_limits<double>::infinity());
    }

    static Value nan()
    {
      return Value(std::numeric_limits<double>::quiet_NaN());
    }

  private:
    void inc(bool reg = true);
    void dec(bool reg = true);

    struct nounop
    {
      template<typename T>
      constexpr T operator()(T&&) const
      {
        throw Value(Error::BadOperand);
      }
    };

    struct nobinop
    {
      template<typename T, typename U>
      constexpr T operator()(T&&, U&&) const
      {
        throw Value(Error::BadOperand);
      }
    };

    template<
      typename OpB,
      typename OpI = OpB,
      typename OpU = OpI,
      typename OpF = OpU>
    Value unop()
    {
      switch (tag)
      {
        case ValueType::Bool:
          return OpB{}(b);

        case ValueType::I8:
          return OpI{}(i8);

        case ValueType::I16:
          return OpI{}(i16);

        case ValueType::I32:
          return OpI{}(i32);

        case ValueType::I64:
          return OpI{}(i64);

        case ValueType::U8:
          return OpU{}(u8);

        case ValueType::U16:
          return OpU{}(u16);

        case ValueType::U32:
          return OpU{}(u32);

        case ValueType::U64:
          return OpU{}(u64);

        case ValueType::ILong:
          return OpI{}(ilong);

        case ValueType::ULong:
          return OpU{}(ulong);

        case ValueType::ISize:
          return OpI{}(isize);

        case ValueType::USize:
          return OpU{}(usize);

        case ValueType::F32:
          return OpF{}(f32);

        case ValueType::F64:
          return OpF{}(f64);

        default:
          throw Value(Error::BadOperand);
      }
    }

    template<
      typename OpB,
      typename OpI = OpB,
      typename OpU = OpI,
      typename OpF = OpU>
    Value binop(Value& v)
    {
      if (tag != v.tag)
        throw Value(Error::MismatchedTypes);

      switch (tag)
      {
        case ValueType::Bool:
          return OpB{}(b, v.b);

        case ValueType::I8:
          return OpI{}(i8, v.i8);

        case ValueType::I16:
          return OpI{}(i16, v.i16);

        case ValueType::I32:
          return OpI{}(i32, v.i32);

        case ValueType::I64:
          return OpI{}(i64, v.i64);

        case ValueType::U8:
          return OpU{}(u8, v.u8);

        case ValueType::U16:
          return OpU{}(u16, v.u16);

        case ValueType::U32:
          return OpU{}(u32, v.u32);

        case ValueType::U64:
          return OpU{}(u64, v.u64);

        case ValueType::ILong:
          return OpI{}(ilong, v.ilong);

        case ValueType::ULong:
          return OpU{}(ulong, v.ulong);

        case ValueType::ISize:
          return OpI{}(isize, v.isize);

        case ValueType::USize:
          return OpU{}(usize, v.usize);

        case ValueType::F32:
          return OpF{}(f32, v.f32);

        case ValueType::F64:
          return OpF{}(f64, v.f64);

        default:
          throw Value(Error::BadOperand);
      }
    }

    template<typename T>
    T get()
    {
      switch (tag)
      {
        case ValueType::I8:
          return i8;

        case ValueType::I16:
          return i16;

        case ValueType::I32:
          return i32;

        case ValueType::I64:
          return i64;

        case ValueType::U8:
          return u8;

        case ValueType::U16:
          return u16;

        case ValueType::U32:
          return u32;

        case ValueType::U64:
          return u64;

        case ValueType::ILong:
          return ilong;

        case ValueType::ULong:
          return ulong;

        case ValueType::ISize:
          return isize;

        case ValueType::USize:
          return usize;

        case ValueType::F32:
          return f32;

        case ValueType::F64:
          return f64;

        default:
          throw Value(Error::BadOperand);
      }
    }

    template<typename T>
    void set(T value)
    {
      switch (tag)
      {
        case ValueType::I8:
          i8 = value;
          break;

        case ValueType::I16:
          i16 = value;
          break;

        case ValueType::I32:
          i32 = value;
          break;

        case ValueType::I64:
          i64 = value;
          break;

        case ValueType::U8:
          u8 = value;
          break;

        case ValueType::U16:
          u16 = value;
          break;

        case ValueType::U32:
          u32 = value;
          break;

        case ValueType::U64:
          u64 = value;
          break;

        case ValueType::ILong:
          ilong = value;
          break;

        case ValueType::ULong:
          ulong = value;
          break;

        case ValueType::ISize:
          isize = value;
          break;

        case ValueType::USize:
          usize = value;
          break;

        case ValueType::F32:
          f32 = value;
          break;

        case ValueType::F64:
          f64 = value;
          break;

        default:
          throw Value(Error::BadOperand);
      }
    }
  };
}
