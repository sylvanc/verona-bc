#include "ffi.h"

#include "array.h"
#include "object.h"
#include "thread.h"

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <uv.h>
#include <vbci.h>

#ifdef _WIN32
#  define VBCI_FFI extern "C" __declspec(dllexport)
#else
#  define VBCI_FFI extern "C" [[gnu::used]] [[gnu::retain]]
#endif

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

static uv_async_t poke;
static uv_async_t keepalive;
static uv_thread_t thread;
static SSL_CTX* ctx = nullptr;

static void do_add_external(verona::rt::Work* work)
{
  verona::rt::Scheduler::add_external_event_source();
  delete work;
}

static void do_remove_external(verona::rt::Work* work)
{
  verona::rt::Scheduler::remove_external_event_source();
  delete work;
}

static void add_external()
{
  uv_async_send(&poke);
  verona::rt::Scheduler::schedule(new verona::rt::Work(do_add_external));
}

static void remove_external()
{
  uv_async_send(&poke);
  verona::rt::Scheduler::schedule(new verona::rt::Work(do_remove_external));
}

static void callback_close(uv_handle_t* handle)
{
  auto cb = reinterpret_cast<Callback*>(uv_handle_get_data(handle));
  delete cb;
  delete handle;
  remove_external();
}

static void callback_alloc(uv_handle_t*, size_t suggested_size, uv_buf_t* buf)
{
  auto arr = Array::create(
    new uint8_t[Array::size_of(suggested_size, ffi_type_uint8.size)],
    Immutable,
    vbci::type::val(ValueType::U8),
    ValueType::U8,
    suggested_size,
    ffi_type_uint8.size);

  buf->base = static_cast<char*>(arr->get_pointer());
  buf->len = suggested_size;
}

static void
callback_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf)
{
  auto cb = reinterpret_cast<Callback*>(
    uv_handle_get_data(reinterpret_cast<uv_handle_t*>(handle)));

  if (nread < 0)
  {
    if (buf->base)
      (reinterpret_cast<Array*>(buf->base) - 1)->dec(false);

    Thread::run_sync(cb->cb_1, cb->arg, Value(ValueType::I32, nread)).drop();
    return;
  }

  Thread::run_sync(
    cb->cb_read, cb->arg, (reinterpret_cast<Array*>(buf->base) - 1))
    .drop();
}

VBCI_FFI Array* getargv()
{
  return Program::get().get_argv();
}

VBCI_FFI void printval(Value& val)
{
  std::cout << val.to_string() << std::endl;
}

VBCI_FFI void* async_create(int type)
{
  auto htype = static_cast<uv_handle_type>(type);
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
  Thread::run_sync(cb->cb_0, cb->arg).drop();
}

VBCI_FFI void async_cb1(uv_handle_t* handle, int status)
{
  auto cb = reinterpret_cast<Callback*>(uv_handle_get_data(handle));
  Thread::run_sync(cb->cb_0, cb->arg, Value(status)).drop();
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
  array_u8->inc(false);
  auto req = new uv_write_t;
  req->data = array_u8;

  uv_buf_t buf{
    .base = static_cast<char*>(array_u8->get_pointer()),
    .len = array_u8->get_size()};

  uv_write(req, handle, &buf, 1, [](uv_write_t* req, int) {
    reinterpret_cast<Array*>(req->data)->dec(false);
    delete req;
  });
}

namespace vbci
{
  struct HEctx
  {
    Value arg;
    Function* cb;
    std::string host;
    std::string port;
    int delay;
    uv_getaddrinfo_t ga;
    uv_timer_t timer;
    std::vector<uv_connect_t> conns;
    std::vector<uv_tcp_t*> socks;
    struct addrinfo* list;
    size_t next;
    size_t results;
    uv_tcp_t* winner;

    HEctx(
      Value&& arg, Function* cb, const char* host, const char* port, int delay)
    : arg(arg),
      cb(cb),
      host(host),
      port(port),
      delay(delay),
      list(nullptr),
      next(0),
      results(0),
      winner(nullptr)
    {
      uv_timer_init(uv_default_loop(), &timer);
      ga.data = this;
      timer.data = this;

      struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP};

      uv_getaddrinfo(uv_default_loop(), &ga, on_resolved, host, port, &hints);
      add_external();
    }

    static void
    on_resolved(uv_getaddrinfo_t* req, int status, struct addrinfo* res)
    {
      HEctx* ctx = static_cast<HEctx*>(req->data);
      ctx->resolved(status, res);
    }

    static void on_tick(uv_timer_t* handle)
    {
      HEctx* ctx = static_cast<HEctx*>(handle->data);
      ctx->attempt();
    }

    static void on_connect(uv_connect_t* req, int status)
    {
      HEctx* ctx = static_cast<HEctx*>(req->data);
      ctx->connect(req, status);
    }

    void resolved(int status, struct addrinfo* res)
    {
      if (status < 0)
      {
        Thread::run_sync(cb, arg, Value::null(), Value(status));
        return;
      }

      list = res;
      size_t count = 0;
      while (res)
      {
        res = res->ai_next;
        count++;
      }

      conns.resize(count);
      socks.resize(count);

      for (auto& conn : conns)
        conn.data = this;

      uv_timer_start(&timer, on_tick, delay, delay);
      attempt();
    }

    void attempt()
    {
      if (winner || (next >= conns.size()))
        return;

      auto tcp = new uv_tcp_t;
      uv_tcp_init(uv_default_loop(), tcp);
      uv_tcp_connect(&conns.at(next), tcp, list[next].ai_addr, on_connect);
      add_external();

      socks.at(next) = tcp;
      next++;
    }

    void connect(uv_connect_t* req, int status)
    {
      results++;

      if ((status == 0) && !winner)
      {
        for (size_t i = 0; i < next; i++)
        {
          if (&conns.at(i) == req)
          {
            winner = socks.at(i);
            break;
          }
        }

        close_all();
        winner->data = new Callback();
        Thread::run_sync(cb, arg, Value(winner), Value(status));
      }

      if (results < next)
        return;

      if (!winner)
      {
        close_all();
        Thread::run_sync(cb, arg, Value::null(), Value(status));
      }

      delete this;
    }

    void close_all()
    {
      uv_timer_stop(&timer);
      uv_close(reinterpret_cast<uv_handle_t*>(&timer), nullptr);
      uv_freeaddrinfo(list);
      list = nullptr;

      for (size_t i = 0; i < next; i++)
      {
        if (socks.at(i) != winner)
        {
          uv_close(
            reinterpret_cast<uv_handle_t*>(socks.at(i)),
            [](uv_handle_t* handle) { delete handle; });
        }

        socks.at(i) = nullptr;
      }

      remove_external();
    }
  };
}

VBCI_FFI bool happy_eyeballs(
  Value& val, Value& f, const char* host, const char* port, int delay)
{
  // The callback takes the arg, a uv_tcp_t*, and an i32 as arguments.
  if (!val.is_sendable() || !f.is_function())
    return false;

  new vbci::HEctx(std::move(val), f.function(), host, port, delay);
  return true;
}

namespace vbci
{
  void run_loop()
  {
    SSL_library_init();
    ctx = SSL_CTX_new(TLS_method());
    SSL_CTX_set_default_verify_paths(ctx);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);

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
    SSL_CTX_free(ctx);
    ctx = nullptr;
  }
}
