#pragma once

#include <uv.h>

#ifdef _WIN32
#  define VBCI_KEEP __declspec(dllexport)
#else
#  define VBCI_KEEP [[gnu::used]] [[gnu::retain]]
#endif

#define VBCI_FFI extern "C" VBCI_KEEP

VBCI_FFI void* async_create(uv_handle_type htype);
VBCI_FFI void async_close(uv_handle_t* handle);

namespace vbci
{
  void start_loop();
  void stop_loop();

  void add_external();
  void remove_external();

  void start_ssl();
  void stop_ssl();

  void callback_close(uv_handle_t* handle);
  void
  callback_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
  void callback_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf);
}
