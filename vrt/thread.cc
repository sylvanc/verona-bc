#include "thread.h"

#include "thread_context.h"
#include "vrt.h"

namespace vrt
{
  void init_thread()
  {
    ThreadContext::init();
  }

  void deinit_thread()
  {
    ThreadContext::deinit();
  }
}

extern "C" VRT_EXPORT vrt_thread* vrt_thread_current(void)
{
  auto* context = vrt::ThreadContext::try_get();
  return context == nullptr ? nullptr : &context->thread;
}

extern "C" VRT_EXPORT vrt_frame* vrt_thread_current_frame(void)
{
  auto* context = vrt::ThreadContext::try_get();
  return context == nullptr ? nullptr : context->thread.frame;
}
