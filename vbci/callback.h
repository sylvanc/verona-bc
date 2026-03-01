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
    Register lambda;
  };

  // Create a callback closure from a lambda and its apply function.
  // The lambda is moved into the closure.
  CallbackClosure* make_callback(Register& lambda, Function* func);

  // Get the C function pointer from a callback closure.
  void* callback_ptr(CallbackClosure* cc);

  // Free a callback closure and release the lambda.
  void free_callback(CallbackClosure* cc);
}
