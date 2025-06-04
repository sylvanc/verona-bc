#include "types.h"

#include "program.h"
#include "value.h"

namespace vbci
{
  bool subtype(
    ComplexType* c1,
    ComplexType* c2,
    size_t ref1 = 0,
    size_t arr1 = 0,
    size_t cown1 = 0,
    size_t ref2 = 0,
    size_t arr2 = 0,
    size_t cown2 = 0)
  {
    bool invariant = false;

    while (true)
    {
      // Every type in c1 must be a subtype of c2.
      if (c1->tag == ComplexType::Union)
      {
        for (auto& child : c1->children)
        {
          if (
            !subtype(&child, c2, ref1, arr1, cown1, ref2, arr2, cown2) ||
            (invariant &&
             !subtype(c2, &child, ref2, arr2, cown2, ref1, arr1, cown1)))
            return false;
        }

        return true;
      }

      // c1 must be a subtype of some type in c2.
      if (c2->tag == ComplexType::Union)
      {
        for (auto& child : c2->children)
        {
          if (subtype(c1, &child, ref1, arr1, cown1, ref2, arr2, cown2))
            return true;
        }

        return false;
      }

      // Our ref depth must be the same.
      if (c1->tag == ComplexType::Tag::Ref)
      {
        ref1 += c1->depth;
        c1 = &c1->children.front();
      }

      if (c2->tag == ComplexType::Tag::Ref)
      {
        ref2 += c2->depth;
        c2 = &c2->children.front();
      }

      if (ref1 != ref2)
        return false;

      // Our array depth must be the same.
      if (c1->tag == ComplexType::Tag::Array)
      {
        arr1 += c1->depth;
        c1 = &c1->children.front();
      }

      if (c2->tag == ComplexType::Tag::Array)
      {
        arr2 += c2->depth;
        c2 = &c2->children.front();
      }

      if (arr1 != arr2)
        return false;

      // Our cown depth must be the same.
      if (c1->tag == ComplexType::Tag::Cown)
      {
        cown1 += c1->depth;
        c1 = &c1->children.front();
      }

      if (c2->tag == ComplexType::Tag::Cown)
      {
        cown2 += c2->depth;
        c2 = &c2->children.front();
      }

      if (cown1 != cown2)
        return false;

      // If the right-hand side is a container, we must be invariant.
      if (ref2 || arr2 || cown2)
        invariant = true;

      // Reset all depths.
      ref1 = arr1 = cown1 = ref2 = arr2 = cown2 = 0;

      // At this point, both c1 and c2 are Base types.
      assert(c1->tag == ComplexType::Tag::Base);
      assert(c2->tag == ComplexType::Tag::Base);

      // If the right-hand side is a dynamic type, succeed.
      if (c2->type_id == type::Dyn)
        return true;

      // If the left-hand side is a dynamic type, fail.
      if (c1->type_id == type::Dyn)
        return false;

      // We have a primitive or class on both sides, and they must match.
      assert(type::is_val(c1->type_id) || type::is_class(c1->type_id));
      assert(type::is_val(c2->type_id) || type::is_class(c2->type_id));
      return c1->type_id == c2->type_id;
    }
  }

  TypeId::TypeId() : tag(+TypeTag::Base), ref(0), idx(type::Dyn) {}

  TypeId TypeId::val(ValueType type)
  {
    if (+type >= NumPrimitiveClasses)
      throw Value(Error::BadType);

    return leb(type::val(type));
  }

  TypeId TypeId::dyn()
  {
    return TypeId();
  }

  TypeId TypeId::cls(size_t idx)
  {
    return leb(type::cls(idx));
  }

  TypeId TypeId::leb(size_t leb)
  {
    TypeId t;
    t.idx = leb;
    return t;
  }

  TypeId TypeId::argv()
  {
    return leb(type::complex(Argv));
  }

  bool TypeId::is_dyn() const
  {
    return is_base() && (idx == type::Dyn);
  }

  bool TypeId::is_val() const
  {
    return is_base() && type::is_val(idx);
  }

  bool TypeId::is_class() const
  {
    return is_base() && type::is_class(idx);
  }

  bool TypeId::is_complex() const
  {
    return is_base() && type::is_complex(idx);
  }

  bool TypeId::is_base() const
  {
    return (tag == +TypeTag::Base) && !ref;
  }

  bool TypeId::is_ref() const
  {
    return ref > 0;
  }

  bool TypeId::is_array() const
  {
    return (tag == +TypeTag::Array) && !ref;
  }

  bool TypeId::is_cown() const
  {
    return (tag == +TypeTag::Cown) && !ref;
  }

  bool TypeId::is_union() const
  {
    return (tag == +TypeTag::Union) && !ref;
  }

  ValueType TypeId::val() const
  {
    if (!is_val())
      throw Value(Error::BadType);

    return static_cast<ValueType>(idx >> type::Shift);
  }

  size_t TypeId::cls() const
  {
    if (!is_class())
      throw Value(Error::BadType);

    return (idx >> type::Shift) - (NumPrimitiveClasses + 5);
  }

  size_t TypeId::complex() const
  {
    if (!is_complex())
      throw Value(Error::BadType);

    return idx >> type::Shift;
  }

  TypeId TypeId::array() const
  {
    if (!is_base())
      throw Value(Error::BadType);

    auto t = *this;
    t.tag = +TypeTag::Array;
    return t;
  }

  TypeId TypeId::unarray() const
  {
    if (!is_array())
      throw Value(Error::BadType);

    auto t = *this;
    t.tag = +TypeTag::Base;
    return t;
  }

  TypeId TypeId::cown() const
  {
    if (!is_base())
      throw Value(Error::BadType);

    auto t = *this;
    t.tag = +TypeTag::Cown;
    return t;
  }

  TypeId TypeId::uncown() const
  {
    if (!is_cown())
      throw Value(Error::BadType);

    auto t = *this;
    t.tag = +TypeTag::Base;
    return t;
  }

  TypeId TypeId::make_ref() const
  {
    if (ref >= MaxRef)
      throw Value(Error::TooManyRefs);

    auto t = *this;
    t.ref = ref + 1;
    return t;
  }

  bool TypeId::operator==(const TypeId& that) const
  {
    return (tag == that.tag) && (ref == that.ref) && (idx == that.idx);
  }

  bool TypeId::operator<(const TypeId& that) const
  {
    ComplexType tmp_this;
    ComplexType tmp_that;
    ComplexType* t1;
    ComplexType* t2;

    if (type::is_complex(idx))
    {
      t1 = &Program::get().complex_type(idx >> type::Shift);
    }
    else
    {
      tmp_this.type_id = idx;
      t1 = &tmp_this;
    }

    if (type::is_complex(that.idx))
    {
      t2 = &Program::get().complex_type(that.idx >> type::Shift);
    }
    else
    {
      tmp_that.type_id = that.idx;
      t2 = &tmp_that;
    }

    return subtype(
      t1,
      t2,
      ref,
      is_array(),
      is_cown(),
      that.ref,
      that.is_array(),
      that.is_cown());
  }
}
