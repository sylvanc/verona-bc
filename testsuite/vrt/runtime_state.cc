#include "context.h"
#include "vrt.h"

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
    (vrt_thread_current_frame(nullptr) != nullptr) ||
    (vrt_frame_enter(nullptr, &root_function) != nullptr) ||
    (vrt_frame_parent(nullptr) != nullptr) || (vrt_frame_id(nullptr) != 0) ||
    (vrt_frame_function(nullptr) != nullptr))
    return 4;

  auto* thread = vrt_thread_create();
  if (thread == nullptr)
    return 5;

  if (vrt_thread_current_frame(thread) != nullptr)
    return 6;

  auto* root = vrt_frame_enter(thread, &root_function);
  if (
    (root == nullptr) || (vrt_thread_current_frame(thread) != root) ||
    (vrt_frame_parent(root) != nullptr) ||
    (vrt_frame_function(root) != &root_function))
    return 7;

  const auto root_id = vrt_frame_id(root);
  if (root_id == 0)
    return 8;

  auto* child = vrt_frame_enter(thread, &child_function);
  if (
    (child == nullptr) || (vrt_thread_current_frame(thread) != child) ||
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
  vrt_frame_prepare_tailcall(thread, child, &tail_function);
  if (
    (vrt_thread_current_frame(thread) != child) ||
    (vrt_frame_parent(child) != root) || (vrt_frame_id(child) != child_id) ||
    (vrt_frame_function(child) != &tail_function) ||
    (child->region != child_region) || (child->stack_mark != 4) ||
    (child->finalizer_mark != 5) || (child->raise_target != root_id))
    return 10;

  vrt_frame_leave(thread, child);
  if (vrt_thread_current_frame(thread) != root)
    return 11;

  vrt_frame_leave(thread, root);
  if (vrt_thread_current_frame(thread) != nullptr)
    return 12;

  vrt_thread_destroy(thread);

  thread = vrt_thread_create();
  if (
    (thread == nullptr) ||
    (vrt_frame_enter(thread, &root_function) == nullptr) ||
    (vrt_frame_enter(thread, &child_function) == nullptr))
    return 13;

  vrt_thread_destroy(thread);
  vrt_thread_destroy(nullptr);

  return 0;
}
