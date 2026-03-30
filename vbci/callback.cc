#include "callback.h"

#include "program.h"
#include "region.h"
#include "thread.h"
#include "value.h"

namespace vbci
{
  // Universal libffi trampoline. Called by C code through the closure's
  // code_ptr. Marshals C arguments back to Verona Values and calls the
  // lambda's apply method via Thread::handle_callback.
  static void
  callback_handler(ffi_cif* /*cif*/, void* ret, void* args[], void* user_data)
  {
    auto* cc = static_cast<CallbackClosure*>(user_data);
    Thread::handle_callback(cc, ret, args);
  }

  CallbackClosure* make_callback(const Register& lambda, Function* func)
  {
    auto* cc = new CallbackClosure();

    // Allocate the ffi_closure.
    cc->closure = static_cast<ffi_closure*>(
      ffi_closure_alloc(sizeof(ffi_closure), &cc->code_ptr));

    if (!cc->closure)
    {
      delete cc;
      Value::error(Error::BadArgs);
    }

    // Build the ffi_cif from the apply function's signature.
    // func->param_types[0] is self (the lambda), skip it.
    // func->param_types[1:] are the actual callback parameters.
    auto& program = Program::get();

    for (size_t i = 1; i < func->param_types.size(); i++)
    {
      auto rep = program.layout_type_id(func->param_types[i]);
      cc->arg_value_types.push_back(rep.first);

      if (rep.first == ValueType::Invalid)
        cc->arg_ffi_types.push_back(program.value_type());
      else
        cc->arg_ffi_types.push_back(rep.second);
    }

    // Return type.
    auto ret_rep = program.layout_type_id(func->return_type);
    cc->return_value_type = ret_rep.first;
    cc->return_ffi_type = ret_rep.second;

    if (cc->return_value_type == ValueType::Invalid)
      cc->return_ffi_type = program.value_type();

    // Prepare the cif.
    if (
      ffi_prep_cif(
        &cc->cif,
        FFI_DEFAULT_ABI,
        static_cast<unsigned>(cc->arg_ffi_types.size()),
        cc->return_ffi_type,
        cc->arg_ffi_types.data()) != FFI_OK)
    {
      ffi_closure_free(cc->closure);
      delete cc;
      Value::error(Error::BadArgs);
    }

    // Prepare the closure.
    if (
      ffi_prep_closure_loc(
        cc->closure, &cc->cif, callback_handler, cc, cc->code_ptr) != FFI_OK)
    {
      ffi_closure_free(cc->closure);
      delete cc;
      Value::error(Error::BadArgs);
    }

    cc->func = func;
    cc->lambda = lambda.borrow();
    return cc;
  }
}
