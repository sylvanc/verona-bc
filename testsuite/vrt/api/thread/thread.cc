#include "vrt.h"
#include <vrt/frame.h>
#include <vrt/thread.h>

#include <thread>

int main()
{
  const vrt::Func root_function{1, "root", nullptr};
  const vrt::Func child_function{2, "child", nullptr};

  if (
    (vrt_thread_current() != nullptr) ||
    (vrt_thread_current_frame() != nullptr))
    return 1;

  vrt::init_thread();
  auto* thread = vrt_thread_current();
  if ((thread == nullptr) || (vrt_thread_current_frame() != nullptr))
    return 2;

  bool isolated = false;
  std::thread worker([&isolated, thread, &root_function]() {
    if (
      (vrt_thread_current() != nullptr) ||
      (vrt_thread_current_frame() != nullptr))
      return;

    vrt::init_thread();
    auto* worker_thread = vrt_thread_current();
    auto* worker_frame = vrt_frame_enter(&root_function);
    isolated = (worker_thread != nullptr) && (worker_thread != thread) &&
      (worker_frame != nullptr) && (vrt_thread_current_frame() == worker_frame);
    vrt::deinit_thread();
    isolated = isolated && (vrt_thread_current() == nullptr);
  });
  worker.join();
  if (!isolated || (vrt_thread_current() != thread))
    return 3;

  if (
    (vrt_frame_enter(&root_function) == nullptr) ||
    (vrt_frame_enter(&child_function) == nullptr))
    return 4;

  vrt::deinit_thread();
  if (
    (vrt_thread_current() != nullptr) ||
    (vrt_thread_current_frame() != nullptr))
    return 5;

  vrt::init_thread();
  thread = vrt_thread_current();
  if (
    (thread == nullptr) || (vrt_frame_enter(&root_function) == nullptr) ||
    (vrt_frame_id(vrt_thread_current_frame()) != 1))
    return 6;

  vrt::deinit_thread();
  return 0;
}