#include "frame.h"
#include "thread.h"
#include "vrt.h"

#include <thread>

int main()
{
  vrt::reset_exit_code();
  if (vrt::get_exit_code() != 0)
    return 1;

  vrt::set_exit_code(7);
  if (vrt::get_exit_code() != 7)
    return 2;

  vrt::set_exit_code(3);
  if (vrt::get_exit_code() != 3)
    return 3;

  const vrt_function_descriptor root_function{1, "root"};
  const vrt_function_descriptor child_function{2, "child"};
  const vrt_function_descriptor tail_function{3, "tail"};

  if (
    (vrt_thread_current() != nullptr) ||
    (vrt_thread_current_frame() != nullptr) ||
    (vrt_frame_parent(nullptr) != nullptr) || (vrt_frame_id(nullptr) != 0) ||
    (vrt_frame_function(nullptr) != nullptr))
    return 4;

  vrt::init_thread();
  auto* thread = vrt_thread_current();
  if (thread == nullptr)
    return 5;

  if (vrt_thread_current_frame() != nullptr)
    return 6;

  auto* root = vrt_frame_enter(&root_function);
  if (
    (root == nullptr) || (vrt_thread_current_frame() != root) ||
    (vrt_frame_parent(root) != nullptr) ||
    (vrt_frame_function(root) != &root_function) ||
    (root->raise_target != vrt_frame_id(root)))
    return 7;

  const auto root_id = vrt_frame_id(root);
  if ((root_id == 0) || (vrt_frame_get_raise_target() != root_id))
    return 8;

  const auto temporary_target = root_id + 1000;
  if (
    (vrt_frame_set_raise_target(temporary_target) != root_id) ||
    (vrt_frame_get_raise_target() != temporary_target) ||
    (vrt_frame_set_raise_target(root_id) != temporary_target) ||
    (vrt_frame_get_raise_target() != root_id))
    return 23;

  auto* child = vrt_frame_enter(&child_function);
  if (
    (child == nullptr) || (vrt_thread_current_frame() != child) ||
    (vrt_frame_parent(child) != root) ||
    (vrt_frame_function(child) != &child_function) ||
    (vrt_frame_id(child) == root_id) ||
    (child->raise_target != vrt_frame_id(child)))
    return 9;

  const auto child_id = vrt_frame_id(child);
  if (
    (vrt_frame_get_raise_target() != child_id) ||
    (vrt_frame_set_raise_target(root_id) != child_id) ||
    (vrt_frame_get_raise_target() != root_id))
    return 24;

  auto* const child_region = reinterpret_cast<vrt_region*>(thread);
  child->region = child_region;
  child->stack_mark = 4;
  child->finalizer_mark = 5;
  vrt_frame_reuse(&tail_function);
  if (
    (vrt_thread_current_frame() != child) ||
    (vrt_frame_parent(child) != root) || (vrt_frame_id(child) != child_id) ||
    (vrt_frame_function(child) != &tail_function) ||
    (child->region != child_region) || (child->stack_mark != 4) ||
    (child->finalizer_mark != 5) || (child->raise_target != root_id))
    return 10;

  vrt_frame_leave();
  if (vrt_thread_current_frame() != root)
    return 11;

  auto* continuation =
    static_cast<std::jmp_buf*>(vrt_frame_raise_continuation());
  if (continuation == nullptr)
    return 20;

  if (setjmp(*continuation) == 0)
  {
    child = vrt_frame_enter(&child_function);
    if (child == nullptr)
      return 21;

    if (vrt_frame_set_raise_target(root_id) != vrt_frame_id(child))
      return 25;

    vrt_frame_raise(42);
  }

  if (
    (vrt_thread_current_frame() != root) ||
    (vrt_frame_take_raised_value() != 42))
    return 22;

  vrt_frame_reuse(nullptr);
  if (
    (vrt_thread_current_frame() != root) ||
    (vrt_frame_function(root) != nullptr))
    return 13;

  vrt_frame_reuse(&tail_function);
  if (
    (vrt_thread_current_frame() != root) || (vrt_frame_parent(root) != nullptr) ||
    (vrt_frame_id(root) != root_id) ||
    (vrt_frame_function(root) != &tail_function))
    return 14;

  vrt_frame_leave();
  if (vrt_thread_current_frame() != nullptr)
    return 15;

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
    return 16;

  if (
    (vrt_frame_enter(&root_function) == nullptr) ||
    (vrt_frame_enter(&child_function) == nullptr))
    return 17;

  vrt::deinit_thread();
  if (
    (vrt_thread_current() != nullptr) ||
    (vrt_thread_current_frame() != nullptr))
    return 18;

  vrt::init_thread();
  thread = vrt_thread_current();
  if (
    (thread == nullptr) || (vrt_frame_enter(&root_function) == nullptr) ||
    (vrt_frame_id(vrt_thread_current_frame()) != 1))
    return 19;

  vrt::deinit_thread();

  return 0;
}
