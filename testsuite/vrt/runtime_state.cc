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
    (vrt_frame_function(root) != &root_function))
    return 7;

  const auto root_id = vrt_frame_id(root);
  if (root_id == 0)
    return 8;

  auto* child = vrt_frame_enter(&child_function);
  if (
    (child == nullptr) || (vrt_thread_current_frame() != child) ||
    (vrt_frame_parent(child) != root) ||
    (vrt_frame_function(child) != &child_function) ||
    (vrt_frame_id(child) == root_id))
    return 9;

  const auto child_id = vrt_frame_id(child);
  auto* const child_region = reinterpret_cast<vrt_region*>(thread);
  child->region = child_region;
  child->stack_mark = 4;
  child->finalizer_mark = 5;
  child->raise_target = root_id;
  vrt_frame_prepare_tailcall(&tail_function);
  if (
    (vrt_thread_current_frame() != child) ||
    (vrt_frame_parent(child) != root) || (vrt_frame_id(child) != child_id) ||
    (vrt_frame_function(child) != &tail_function) ||
    (child->region != child_region) || (child->stack_mark != 4) ||
    (child->finalizer_mark != 5) || (child->raise_target != root_id))
    return 10;

  auto* reused = vrt_frame_enter(&tail_function);
  if (
    (reused != child) || (vrt_thread_current_frame() != child) ||
    (vrt_frame_parent(child) != root) || (vrt_frame_id(child) != child_id) ||
    (vrt_frame_function(child) != &tail_function) ||
    (child->region != child_region) || (child->stack_mark != 4) ||
    (child->finalizer_mark != 5) || (child->raise_target != root_id))
    return 11;

  vrt_frame_leave();
  if (vrt_thread_current_frame() != root)
    return 12;

  vrt_frame_prepare_tailcall(nullptr);
  if (
    (vrt_thread_current_frame() != root) ||
    (vrt_frame_function(root) != nullptr))
    return 13;

  auto* dynamically_reused = vrt_frame_enter(&tail_function);
  if (
    (dynamically_reused != root) || (vrt_thread_current_frame() != root) ||
    (vrt_frame_parent(root) != nullptr) || (vrt_frame_id(root) != root_id) ||
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
