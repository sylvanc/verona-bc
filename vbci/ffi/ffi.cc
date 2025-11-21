#include "ffi.h"

#include "../array.h"
#include "../object.h"
#include "../thread.h"
#include "../value.h"

#include <verona.h>

using namespace vbci;

struct Callback
{
  Value arg;
  Function* cb_0;
  Function* cb_1;
  Function* cb_read;
  Function* cb_read_udp;

  Callback()
  : arg(Value::none()),
    cb_0(nullptr),
    cb_1(nullptr),
    cb_read(nullptr),
    cb_read_udp(nullptr)
  {}

  ~Callback()
  {
    arg.drop();
  }
};

VBCI_FFI bool platform_is_mac()
{
  return platform::is_mac();
}

VBCI_FFI bool platform_is_linux()
{
  return platform::is_linux();
}

VBCI_FFI bool platform_is_windows()
{
  return platform::is_windows();
}

VBCI_FFI Array* getargv()
{
  return Program::get().get_argv();
}

VBCI_FFI void printval(Value& val)
{
  std::cout << val.to_string() << std::endl;
}

VBCI_FFI void* async_create(uv_handle_type htype)
{
  auto size = uv_handle_size(htype);

  if (size == size_t(-1))
    return nullptr;

  add_external();
  auto h = reinterpret_cast<uv_handle_t*>(new uint8_t[size]);

  switch (htype)
  {
    case UV_CHECK:
      uv_check_init(uv_default_loop(), reinterpret_cast<uv_check_t*>(h));
      break;

    case UV_FS_EVENT:
      uv_fs_event_init(uv_default_loop(), reinterpret_cast<uv_fs_event_t*>(h));
      break;

    case UV_FS_POLL:
      uv_fs_poll_init(uv_default_loop(), reinterpret_cast<uv_fs_poll_t*>(h));
      break;

    case UV_IDLE:
      uv_idle_init(uv_default_loop(), reinterpret_cast<uv_idle_t*>(h));
      break;

    case UV_PREPARE:
      uv_prepare_init(uv_default_loop(), reinterpret_cast<uv_prepare_t*>(h));
      break;

    case UV_TCP:
      uv_tcp_init(uv_default_loop(), reinterpret_cast<uv_tcp_t*>(h));
      break;

    case UV_TIMER:
      uv_timer_init(uv_default_loop(), reinterpret_cast<uv_timer_t*>(h));
      break;

    case UV_UDP:
      uv_udp_init(uv_default_loop(), reinterpret_cast<uv_udp_t*>(h));
      break;

    case UV_SIGNAL:
      uv_signal_init(uv_default_loop(), reinterpret_cast<uv_signal_t*>(h));
      break;

    default:
      // Not initialized: async, pipe, poll, process, tty.
      break;
  }

  h->data = new Callback();
  return h;
}

VBCI_FFI void async_close(uv_handle_t* handle)
{
  if (!handle)
    return;

  if (handle->type == UV_TCP)
  {
    auto stream = reinterpret_cast<uv_stream_t*>(handle);
    auto req = new uv_shutdown_t;
    req->data = handle;

    uv_shutdown(req, stream, [](uv_shutdown_t* req, int) {
      auto handle = reinterpret_cast<uv_handle_t*>(req->data);
      uv_close(handle, callback_close);
      delete req;
    });
  }
  else
  {
    uv_close(handle, callback_close);
  }
}

VBCI_FFI bool async_set_arg(uv_handle_t* handle, Value& val)
{
  if (!val.is_sendable())
    return false;

  Callback* cb = static_cast<Callback*>(uv_handle_get_data(handle));
  cb->arg = std::move(val);
  return true;
}

VBCI_FFI bool async_set_cb0(uv_handle_t* handle, Value& f)
{
  // cb0 takes only the arg as an argument.
  if (!f.is_function())
    return false;

  Callback* cb = static_cast<Callback*>(uv_handle_get_data(handle));
  cb->cb_0 = f.function();
  return true;
}

VBCI_FFI bool async_set_cb1(uv_handle_t* handle, Value& f)
{
  // cb1 takes the arg and an i32 as arguments.
  if (!f.is_function())
    return false;

  Callback* cb = static_cast<Callback*>(uv_handle_get_data(handle));
  cb->cb_1 = f.function();
  return true;
}

VBCI_FFI void async_cb0(uv_handle_t* handle)
{
  auto cb = reinterpret_cast<Callback*>(uv_handle_get_data(handle));
  Thread::run_sync(cb->cb_0, std::move(cb->arg));
}

VBCI_FFI void async_cb1(uv_handle_t* handle, int status)
{
  auto cb = reinterpret_cast<Callback*>(uv_handle_get_data(handle));
  Thread::run_sync(cb->cb_0, std::move(cb->arg), Value(status));
}

VBCI_FFI bool async_read_start(uv_stream_t* handle, Value& f)
{
  // cb_read takes the arg and a u8[] as arguments.
  if (!f.is_function())
    return false;

  Callback* cb = static_cast<Callback*>(
    uv_handle_get_data(reinterpret_cast<uv_handle_t*>(handle)));
  cb->cb_read = f.function();
  uv_read_start(handle, callback_alloc, callback_read);
  return true;
}

VBCI_FFI void async_write(uv_stream_t* handle, Array* array_u8)
{
  array_u8->inc<false>();
  auto req = new uv_write_t;
  req->data = array_u8;

  uv_buf_t buf{
    .base = static_cast<char*>(array_u8->get_pointer()),
    .len = array_u8->get_size()};

  uv_write(req, handle, &buf, 1, [](uv_write_t* req, int) {
    reinterpret_cast<Array*>(req->data)->dec<false>();
    delete req;
  });
}

namespace vbci
{
#if !defined(PLATFORM_IS_WINDOWS)
  static uv_signal_t sigpipe_h;
#endif

  static uv_async_t poke;
  static uv_async_t keepalive;
  static uv_thread_t thread;

  void start_loop()
  {
    start_ssl();

#if !defined(PLATFORM_IS_WINDOWS)
    // Ignore SIGPIPE.
    uv_signal_init(uv_default_loop(), &sigpipe_h);
    uv_signal_start(&sigpipe_h, [](uv_signal_t*, int) {}, SIGPIPE);
    uv_unref(reinterpret_cast<uv_handle_t*>(&sigpipe_h));
#endif

    uv_async_init(uv_default_loop(), &keepalive, [](uv_async_t*) {
      uv_close(reinterpret_cast<uv_handle_t*>(&poke), nullptr);
      uv_close(reinterpret_cast<uv_handle_t*>(&keepalive), nullptr);
    });

    uv_async_init(uv_default_loop(), &poke, [](uv_async_t*) {});

    uv_thread_create(
      &thread,
      [](void*) { uv_run(uv_default_loop(), UV_RUN_DEFAULT); },
      nullptr);
  }

  void stop_loop()
  {
    uv_async_send(&keepalive);
    uv_thread_join(&thread);
    uv_loop_close(uv_default_loop());
    stop_ssl();
  }

  static void do_add_external(verona::rt::Work* work)
  {
    uv_async_send(&poke);
    verona::rt::Scheduler::add_external_event_source();
    delete work;
  }

  static void do_remove_external(verona::rt::Work* work)
  {
    uv_async_send(&poke);
    verona::rt::Scheduler::remove_external_event_source();
    delete work;
  }

  void add_external()
  {
    verona::rt::Scheduler::schedule(new verona::rt::Work(do_add_external));
  }

  void remove_external()
  {
    verona::rt::Scheduler::schedule(new verona::rt::Work(do_remove_external));
  }

  void callback_close(uv_handle_t* handle)
  {
    auto cb = reinterpret_cast<Callback*>(uv_handle_get_data(handle));

    if (cb)
      delete cb;

    delete[] reinterpret_cast<uint8_t*>(handle);
    remove_external();
  }

  void callback_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
  {
    (void)handle;

    auto arr = Array::create(
      new uint8_t[Array::size_of(suggested_size, ffi_type_uint8.size)],
      loc::Immutable,
      Program::get().get_typeid_arg(),
      ValueType::U8,
      suggested_size,
      ffi_type_uint8.size);

    buf->base = static_cast<char*>(arr->get_pointer());
    buf->len = suggested_size;
  }

  void callback_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf)
  {
    auto cb = reinterpret_cast<Callback*>(
      uv_handle_get_data(reinterpret_cast<uv_handle_t*>(handle)));

    if (nread < 0)
    {
      if (buf->base)
        (reinterpret_cast<Array*>(buf->base) - 1)->dec<false>();

      Thread::run_sync(cb->cb_1, std::move(cb->arg), Value(ValueType::I32, nread));
      return;
    }

    auto array = reinterpret_cast<Array*>(buf->base) - 1;
    array->set_size(nread);
    Thread::run_sync(cb->cb_read, std::move(cb->arg), array);
  }
}
