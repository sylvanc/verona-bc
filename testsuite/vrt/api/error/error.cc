#include "vrt.h"

#include <cstdint>
#include <cstring>
#include <vrt/error.h>
#include <vrt/frame.h>
#include <vrt/thread.h>

namespace
{
  void test_entry() {}

  struct RaiseErrorContext
  {
    const vrt_func* func;
  };

  void complete_normally(void* context)
  {
    (*static_cast<uint32_t*>(context))++;
  }

  void fail_invocation(void* context)
  {
    auto* error_context = static_cast<RaiseErrorContext*>(context);
    vrt_frame_enter(error_context->func);
    vrt_error_raise(VRT_ERROR_BAD_ARRAY_INDEX);
  }
}

int main()
{
  const vrt_func root_function{1, "root", test_entry};
  const vrt_func child_function{2, "child", test_entry};

  vrt::init_thread();
  vrt_error_info error{VRT_ERROR_BAD_STORE, &root_function, 42};
  uint32_t calls = 0;
  if (
    !vrt_try_invoke(complete_normally, &calls, &error) || (calls != 1) ||
    (error.code != VRT_ERROR_NONE) || (error.func != nullptr) ||
    (error.site != 0))
    return 1;

  RaiseErrorContext context{&child_function};
  if (
    vrt_try_invoke(fail_invocation, &context, &error) ||
    (error.code != VRT_ERROR_BAD_ARRAY_INDEX) ||
    (error.func != &child_function) || (error.site != 0) ||
    (vrt_thread_current_frame() != nullptr) ||
    (std::strcmp(vrt_error_message(error.code), "bad array index") != 0))
    return 2;

  vrt::deinit_thread();
  return 0;
}
