#include "callback.h"

#include "program.h"
#include "thread.h"
#include "value.h"

namespace vbci
{
  // Universal libffi trampoline. Called by C code through the closure's
  // code_ptr. Marshals C arguments back to Verona Values and calls the
  // lambda's apply method via Thread::run_callback.
  static void
  callback_handler(ffi_cif* /*cif*/, void* ret, void* args[], void* user_data)
  {
    auto* cc = static_cast<CallbackClosure*>(user_data);

    // The apply function's first parameter is self (the lambda).
    // The remaining parameters correspond to the callback's C arguments.
    auto num_c_args = cc->arg_value_types.size();

    // Arg 0: the lambda (self). Borrow it — the closure owns it.
    Thread::set_callback_arg(0, cc->lambda.borrow());

    // Args 1..N: converted C arguments.
    for (size_t i = 0; i < num_c_args; i++)
    {
      auto vt = cc->arg_value_types[i];

      if (vt == ValueType::Invalid)
      {
        // Dynamic type — the C side passed a Value*.
        auto* val = static_cast<Value*>(args[i]);
        Thread::set_callback_arg(i + 1, ValueBorrow(*val));
      }
      else
      {
        Thread::set_callback_arg(i + 1, Value::from_addr(vt, args[i]));
      }
    }

    auto result = Thread::run_callback(cc->func, num_c_args + 1);

    // Marshal the return value back to C.
    if (cc->return_value_type == ValueType::None)
    {
      // void return — nothing to write.
    }
    else if (cc->return_value_type == ValueType::Invalid)
    {
      // Dynamic return — write the full Value.
      *static_cast<Value*>(ret) = result.extract();
    }
    else
    {
      // Primitive return — write via to_addr.
      result.borrow().to_addr(cc->return_value_type, ret);
    }
  }

  CallbackClosure* make_callback(Register& lambda, Function* func)
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

    // Store the function and move the lambda into the closure.
    cc->func = func;
    cc->lambda = std::move(lambda);

    return cc;
  }

  void* callback_ptr(CallbackClosure* cc)
  {
    return cc->code_ptr;
  }

  void free_callback(CallbackClosure* cc)
  {
    ffi_closure_free(cc->closure);
    cc->lambda.clear();
    delete cc;
  }
}
