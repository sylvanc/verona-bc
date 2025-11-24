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

    Register(Value&& v) : Value(std::move(v))
    {
      // Should perform a stack inc here?
    }

    ~Register()
    {
      // Need to drop the value before the stack dec.
      // This needs to perform a stack dec here.
      drop();
    }
  };
} // namespace vbci