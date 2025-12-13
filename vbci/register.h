#pragma once

/**
 *  Implementation of a register in the virtual machine.
 * 
 * This holds a value and manages the ownership semantics of that value.
 */
#include "value.h"

namespace vbci
{
  struct Register : public Value
  {  
  public:
    Register() = default;

    // This requires the value to already have both a classic RC to the underlying
    // dynamically allocated object if there is one, and a stack RC to the region if
    // it is a proper region reference.
    explicit Register(Value&& v) : Value(std::move(v))
    {
      // Should perform a stack inc here?
    }

    Register(Register&& v) = default;

    ~Register()
    {
      drop_reg();
    }

    void operator=(Value&& v)
    {
      drop_reg();
      Value::operator=(std::move(v));
    }

    void operator=(Register&& v)
    {
      if (this == &v)
        return;

      drop_reg();
      Value::operator=(std::move(v));
    }

    void operator=(const Value& v)
    {
      drop_reg();
      *this = v.copy_reg();
    }
  };
} // namespace vbci