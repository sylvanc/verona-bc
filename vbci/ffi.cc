#include "array.h"
#include "object.h"
#include "thread.h"

#include <uv.h>

#ifdef _WIN32
#  define VBCI_FFI extern "C" __declspec(dllexport)
#else
#  define VBCI_FFI extern "C" [[gnu::used]] [[gnu::retain]]
#endif

VBCI_FFI vbci::Array* getargv()
{
  return vbci::Program::get().get_argv();
}

VBCI_FFI void printval(vbci::Value& val)
{
  std::cout << val.to_string() << std::endl;
}

VBCI_FFI void uv_handle_closure(uv_handle_t* handle, vbci::Value& val)
{
  auto obj = val.get_object();

  if (!obj->sendable())
    throw vbci::Value(vbci::Error::BadMethodTarget);

  if (!obj->method(vbci::ApplyMethodId))
    throw vbci::Value(vbci::Error::MethodNotFound);

  auto keep = val;
  uv_handle_set_data(handle, obj);
}

VBCI_FFI void uv_callback_closure(uv_handle_t* handle)
{
  auto obj = reinterpret_cast<vbci::Object*>(uv_handle_get_data(handle));
  vbci::Thread::run(obj).drop();
}

VBCI_FFI void uv_close_closure(uv_handle_t* handle)
{
  vbci::Value obj = reinterpret_cast<vbci::Object*>(uv_handle_get_data(handle));
  obj.drop();
  std::free(handle);
}

namespace vbci
{
  static uv_async_t keepalive;
  static uv_thread_t thread;

  static void loop_thread(void*)
  {
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    uv_loop_close(uv_default_loop());
  }

  void run_loop()
  {
    uv_async_init(uv_default_loop(), &keepalive, [](uv_async_t* handle) {
      uv_close((uv_handle_t*)handle, nullptr);
    });

    uv_thread_create(&thread, loop_thread, nullptr);
  }

  void stop_loop()
  {
    uv_async_send(&keepalive);
    uv_thread_join(&thread);
  }
}
