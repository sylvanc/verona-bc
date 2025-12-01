#include "../thread.h"
#include "../value.h"
#include "ffi.h"

#include <vector>

namespace vbci
{
  struct Eyeballs
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
    struct addrinfo* addr_head;
    struct addrinfo* addr_curr;
    size_t next;
    size_t results;
    uv_tcp_t* winner;

    Eyeballs(
      Value&& arg, Function* cb, const char* host, const char* port, int delay)
    : arg(std::move(arg)),
      cb(cb),
      host(host),
      port(port),
      delay(delay),
      addr_head(nullptr),
      addr_curr(nullptr),
      next(0),
      results(0),
      winner(nullptr)
    {
      uv_timer_init(uv_default_loop(), &timer);
      ga.data = this;
      timer.data = this;

      struct addrinfo hints = {
        .ai_flags = 0,
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
        .ai_addrlen = 0,
        .ai_addr = nullptr,
        .ai_canonname = nullptr,
        .ai_next = nullptr};

      uv_getaddrinfo(uv_default_loop(), &ga, on_resolved, host, port, &hints);
      add_external();
    }

    static void
    on_resolved(uv_getaddrinfo_t* req, int status, struct addrinfo* res)
    {
      Eyeballs* ctx = static_cast<Eyeballs*>(req->data);
      ctx->resolved(status, res);
    }

    static void on_tick(uv_timer_t* handle)
    {
      Eyeballs* ctx = static_cast<Eyeballs*>(handle->data);
      ctx->attempt();
    }

    static void on_connect(uv_connect_t* req, int status)
    {
      Eyeballs* ctx = static_cast<Eyeballs*>(req->data);
      ctx->connect(req, status);
    }

    void resolved(int status, struct addrinfo* res)
    {
      if (status < 0)
      {
        Thread::run_sync(cb, arg.copy(), Value::null(), Value(status));
        return;
      }

      addr_head = res;
      addr_curr = res;
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

      auto tcp = static_cast<uv_tcp_t*>(async_create(UV_TCP));
      uv_tcp_connect(&conns.at(next), tcp, addr_curr->ai_addr, on_connect);

      socks.at(next) = tcp;
      addr_curr = addr_curr->ai_next;
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
        Thread::run_sync(cb, arg.copy(), Value(winner), Value(status));
      }

      if (results < next)
        return;

      if (!winner)
      {
        close_all();
        Thread::run_sync(cb, arg.copy(), Value::null(), Value(status));
      }

      delete this;
    }

    void close_all()
    {
      uv_timer_stop(&timer);
      uv_close(reinterpret_cast<uv_handle_t*>(&timer), nullptr);
      uv_freeaddrinfo(addr_head);
      addr_head = nullptr;
      addr_curr = nullptr;

      for (size_t i = 0; i < next; i++)
      {
        if (socks.at(i) != winner)
          uv_close(reinterpret_cast<uv_handle_t*>(socks.at(i)), callback_close);

        socks.at(i) = nullptr;
      }

      remove_external();
    }
  };
}

VBCI_FFI bool happy_eyeballs(
  vbci::Value& val,
  vbci::Value& f,
  const char* host,
  const char* port,
  int delay)
{
  // The callback takes the arg, a uv_tcp_t*, and an i32 as arguments.
  if (!val.is_sendable() || !f.is_function())
    return false;

  new vbci::Eyeballs(std::move(val), f.function(), host, port, delay);
  return true;
}
