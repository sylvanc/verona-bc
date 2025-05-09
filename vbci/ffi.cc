#include "ffi.h"

#include "array.h"
#include "object.h"
#include "thread.h"

#include <uv.h>

#ifdef _WIN32
#  define VBCI_FFI extern "C" __declspec(dllexport)
#else
#  define VBCI_FFI extern "C" [[gnu::used]] [[gnu::retain]]
#endif

using namespace vbci;

VBCI_FFI Array* getargv()
{
  return Program::get().get_argv();
}

VBCI_FFI void printval(Value& val)
{
  std::cout << val.to_string() << std::endl;
}

VBCI_FFI void* uv_handle_create(int type)
{
  auto size = uv_handle_size(static_cast<uv_handle_type>(type));

  if (size == size_t(-1))
    throw Value(Error::BadType);

  verona::rt::Scheduler::schedule(new verona::rt::Work(add_external));
  return std::malloc(size);
}

VBCI_FFI void uv_handle_closure(uv_handle_t* handle, Value& val)
{
  auto obj = val.get_object();

  if (!obj->sendable())
    throw Value(Error::BadMethodTarget);

  if (!obj->method(vbci::ApplyMethodId))
    throw Value(Error::MethodNotFound);

  auto keep = val;
  uv_handle_set_data(handle, obj);
}

VBCI_FFI void uv_callback_closure(uv_handle_t* handle)
{
  auto obj = reinterpret_cast<Object*>(uv_handle_get_data(handle));
  Thread::run(obj).drop();
}

VBCI_FFI void uv_close_closure(uv_handle_t* handle)
{
  Value obj = reinterpret_cast<Object*>(uv_handle_get_data(handle));
  obj.drop();
  std::free(handle);
  verona::rt::Scheduler::schedule(new verona::rt::Work(remove_external));
}

namespace vbci
{
  static uv_async_t keepalive;
  static uv_thread_t thread;

  void run_loop()
  {
    uv_async_init(uv_default_loop(), &keepalive, [](uv_async_t* handle) {
      uv_close(reinterpret_cast<uv_handle_t*>(handle), nullptr);
    });

    uv_thread_create(
      &thread,
      [](void*) {
        // TODO: default run hangs when setting external event source.
        // uv_run(uv_default_loop(), UV_RUN_DEFAULT);
        while (uv_run(uv_default_loop(), UV_RUN_NOWAIT) != 0)
          ;
      },
      nullptr);
  }

  void stop_loop()
  {
    uv_async_send(&keepalive);
    uv_thread_join(&thread);
    uv_loop_close(uv_default_loop());
  }

  void add_external(verona::rt::Work* work)
  {
    verona::rt::Scheduler::add_external_event_source();
    delete work;
  }

  void remove_external(verona::rt::Work* work)
  {
    verona::rt::Scheduler::remove_external_event_source();
    delete work;
  }
}
