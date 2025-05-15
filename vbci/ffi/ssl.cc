#include "../array.h"
#include "ffi.h"

#include <algorithm>
#include <deque>
#include <openssl/err.h>
#include <openssl/ssl.h>

namespace vbci
{
  static SSL_CTX* ctx = nullptr;

  void start_ssl()
  {
    if (ctx)
      return;

    SSL_library_init();
    ctx = SSL_CTX_new(TLS_method());
    SSL_CTX_set_default_verify_paths(ctx);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
  }

  void stop_ssl()
  {
    if (ctx)
    {
      SSL_CTX_free(ctx);
      ctx = nullptr;
    }
  }

  struct SSLconn
  {
    static constexpr size_t ChunkSize = 4096;
    static constexpr size_t NumWriteBuffers = 16;

    uv_tcp_t* tcp;
    SSL* ssl;
    BIO* rbio;
    BIO* wbio;
    std::deque<Array*> plaintext;
    std::deque<Array*> write_buffers;
    uv_buf_t last_read;
    bool handshake_done;

    SSLconn(uv_tcp_t* tcp) : tcp(tcp)
    {
      ssl = SSL_new(ctx);
      rbio = BIO_new(BIO_s_mem());
      wbio = BIO_new(BIO_s_mem());
      SSL_set_bio(ssl, rbio, wbio);
      last_read = uv_buf_init(nullptr, 0);
      handshake_done = false;
    }

    ~SSLconn()
    {
      async_close(reinterpret_cast<uv_handle_t*>(tcp));

      if (ssl)
      {
        SSL_shutdown(ssl);
        SSL_free(ssl);
      }

      for (auto array : plaintext)
        array->dec(false);

      if (last_read.base)
        (reinterpret_cast<Array*>(last_read.base) - 1)->dec(false);
    }

    void client(const char* hostname)
    {
      SSL_set_connect_state(ssl);
      X509_VERIFY_PARAM* vp = SSL_get0_param(ssl);
      X509_VERIFY_PARAM_set1_host(vp, hostname, 0);
      SSL_set_tlsext_host_name(ssl, hostname);
    }

    int read(Array* array_u8)
    {
      BIO_write(rbio, array_u8->get_pointer(), array_u8->get_size());
      return drive();
    }

    void write(Array* array_u8)
    {
      if (!handshake_done) [[unlikely]]
      {
        write_buffers.push_back(array_u8);
        return;
      }

      SSL_write(ssl, array_u8->get_pointer(), array_u8->get_size());
      flush_writes();
    }

    Array* get_plaintext()
    {
      if (plaintext.empty())
        return nullptr;

      auto array = plaintext.front();
      plaintext.pop_front();
      return array;
    }

    int drive()
    {
      while (!handshake_done)
      {
        int r = SSL_do_handshake(ssl);

        if (r == 1)
        {
          // Handshake completed successfully.
          // TODO: check the certificate.
          // client vs. server mode.
          if (SSL_get_verify_result(ssl) != X509_V_OK)
            return UV_ECONNABORTED;

          flush_writes();
          handshake_done = true;

          while (!write_buffers.empty())
          {
            auto array = write_buffers.front();
            write_buffers.pop_front();
            SSL_write(ssl, array->get_pointer(), array->get_size());
          }

          flush_writes();
          break;
        }

        r = SSL_get_error(ssl, r);

        if (r == SSL_ERROR_WANT_READ)
        {
          // Handshake is waiting for data to be read.
          flush_writes();
          return 0;
        }

        if (r == SSL_ERROR_WANT_WRITE)
        {
          // Handshake is waiting for data to be written.
          flush_writes();
          continue;
        }

        // Handshake failed.
        return UV_ECONNABORTED;
      }

      while (true)
      {
        if (!last_read.base)
          callback_alloc(
            reinterpret_cast<uv_handle_t*>(tcp), ChunkSize, &last_read);

        int n = SSL_read(ssl, last_read.base, last_read.len);

        if (n > 0) [[likely]]
        {
          auto array = reinterpret_cast<Array*>(last_read.base) - 1;
          array->set_size(n);
          plaintext.push_back(array);
          last_read.base = nullptr;
          continue;
        }

        int err = SSL_get_error(ssl, n);

        switch (err)
        {
          // The read operation would block.
          case SSL_ERROR_WANT_READ:
            return plaintext.size();

          // The write operation would block.
          case SSL_ERROR_WANT_WRITE:
          {
            flush_writes();
            continue;
          }

          // Graceful shutdown.
          case SSL_ERROR_ZERO_RETURN:
          {
            flush_writes();
            return UV_EOF;
          }

          // Non-graceful shutdown.
          default:
          {
            unsigned long e;

            while ((e = ERR_get_error()) != 0)
            {
              char txt[256];
              ERR_error_string_n(e, txt, sizeof(txt));
              std::cerr << "TLS error: " << txt << std::endl;
            }

            return UV_ECONNABORTED;
          }
        }
      }

      flush_writes();
      return plaintext.size();
    }

    void flush_writes()
    {
      while ((BIO_pending(wbio) != 0) && (tcp->write_queue_size == 0))
      {
        // Avoid any copies when possible.
        char* data;
        auto len = BIO_get_mem_data(wbio, &data);

        if (len <= 0)
          break;

        uv_buf_t buf = uv_buf_init(data, len);
        auto n = uv_try_write(reinterpret_cast<uv_stream_t*>(tcp), &buf, 1);

        // If this write would block, don't commit the BIO read.
        if (n < 0)
          break;

        BIO_seek(wbio, n);

        // We weren't able to write all the data synchronously.
        if (n < len)
          break;
      }

      if (BIO_pending(wbio) == 0)
        return;

      // Build up buffers to write.
      uv_buf_t stack_bufs[NumWriteBuffers];
      size_t have = 0;

      while ((have < NumWriteBuffers) && (BIO_pending(wbio) != 0))
      {
        size_t chunk = std::min<long>(ChunkSize, BIO_pending(wbio));
        char* p = new char[chunk];

        int n = BIO_read(wbio, p, static_cast<int>(chunk));
        if (n <= 0)
        {
          delete[] p;
          break;
        }

        stack_bufs[have++] = uv_buf_init(p, n);
      }

      // Copy the buffers to the heap.
      auto bufs = new uv_buf_t[have];
      std::copy_n(stack_bufs, have, bufs);

      auto req = new uv_write_t;
      req->data = bufs;

      uv_write(
        req,
        reinterpret_cast<uv_stream_t*>(tcp),
        bufs,
        static_cast<unsigned>(have),
        [](uv_write_t* req, int status) {
          (void)status;
          auto bufs = static_cast<uv_buf_t*>(req->data);

          for (unsigned i = 0; i < req->nbufs; ++i)
            delete[] bufs[i].base;

          delete[] bufs;
          delete req;
        });
    }
  };
}

VBCI_FFI vbci::SSLconn* ssl_client(uv_tcp_t* tcp, const char* hostname)
{
  if (!vbci::ctx)
    return nullptr;

  auto conn = new vbci::SSLconn(tcp);
  conn->client(hostname);
  conn->drive();
  // TODO:
  // SSL_CTX_set1_sigalgs_list
  // check additional X.509 properties?
  // server: SNI to present cert?
  return conn;
}

VBCI_FFI void ssl_close(vbci::SSLconn* ssl)
{
  if (ssl)
    delete ssl;
}

VBCI_FFI int ssl_read(vbci::SSLconn* ssl, vbci::Array* array_u8)
{
  return ssl->read(array_u8);
}

VBCI_FFI void ssl_write(vbci::SSLconn* ssl, vbci::Array* array_u8)
{
  ssl->write(array_u8);
}

VBCI_FFI vbci::ValueBits ssl_plaintext(vbci::SSLconn* ssl)
{
  auto arr = ssl->get_plaintext();

  if (arr)
    return vbci::Value(arr);

  return vbci::Value::none();
}
