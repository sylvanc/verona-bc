#pragma once

#include "function.h"
#include "location.h"
#include "register.h"

#include <ffi.h>
#include <vector>

namespace vbci
{
  struct CallbackClosure
  {
    ffi_closure* closure;
    void* code_ptr;
    ffi_cif cif;
    std::vector<ffi_type*> arg_ffi_types;
    std::vector<ValueType> arg_value_types;
    ffi_type* return_ffi_type;
    ValueType return_value_type;
    Function* func;
    // Borrowed self value. The enclosing Verona _builtin::ffi::callback object
    // owns the callable and is responsible for freeing this closure before the
    // callable goes away.
    Value lambda;

    // Free a callback closure. The borrowed lambda is owned by the enclosing
    // Verona callback object.
    void free()
    {
      ffi_closure_free(closure);
      delete this;
    }
  };

  // Create a callback closure from a lambda and its apply function.
  CallbackClosure* make_callback(const Register& lambda, Function* func);
}
