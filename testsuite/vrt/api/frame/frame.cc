#include "frame.h"

#include <vrt/thread.h>

#include <csetjmp>

int main()
{
  const vrt::Func root_function{1, "root", nullptr};
  const vrt::Func child_function{2, "child", nullptr};
  const vrt::Func tail_function{3, "tail", nullptr};

  if (
    (vrt_frame_parent(nullptr) != nullptr) || (vrt_frame_id(nullptr) != 0) ||
    (vrt_frame_func(nullptr) != nullptr))
    return 1;

  vrt_thread_init();
  if (vrt_thread_current_frame() != nullptr)
    return 2;

  auto* root = vrt_frame_enter(&root_function);
  if (
    (root == nullptr) || (vrt_thread_current_frame() != root) ||
    (vrt_frame_parent(root) != nullptr) ||
    (vrt_frame_func(root) != &root_function) || (root->region == nullptr) ||
    (root->raise_target != root->frame_id))
    return 3;

  const auto root_id = vrt_frame_id(root);
  const auto root_target = root->frame_id.raw();
  if (
    (root_id == 0) || (root_id != root_target) ||
    (vrt_frame_get_raise_target() != root_target))
    return 4;

  const auto temporary_target = root_target + 1000;
  if (
    (vrt_frame_set_raise_target(temporary_target) != root_target) ||
    (vrt_frame_get_raise_target() != temporary_target) ||
    (vrt_frame_set_raise_target(root_target) != temporary_target) ||
    (vrt_frame_get_raise_target() != root_target))
    return 5;

  auto* child = vrt_frame_enter(&child_function);
  if (
    (child == nullptr) || (vrt_thread_current_frame() != child) ||
    (vrt_frame_parent(child) != root) ||
    (vrt_frame_func(child) != &child_function) ||
    (vrt_frame_id(child) == root_id) || (child->region == nullptr) ||
    (child->raise_target != child->frame_id))
    return 6;

  const auto child_id = vrt_frame_id(child);
  const auto child_target = child->frame_id.raw();
  if (
    (child_id != child_target) ||
    (vrt_frame_get_raise_target() != child_target) ||
    (vrt_frame_set_raise_target(root_target) != child_target) ||
    (vrt_frame_get_raise_target() != root_target))
    return 7;

  auto* const child_region = child->region;
  child->stack_mark = 4;
  child->finalizer_mark = 5;
  vrt_frame_reuse(&tail_function);
  if (
    (vrt_thread_current_frame() != child) ||
    (vrt_frame_parent(child) != root) || (vrt_frame_id(child) != child_id) ||
    (vrt_frame_func(child) != &tail_function) ||
    (child->region != child_region) || (child->stack_mark != 4) ||
    (child->finalizer_mark != 5) || (child->raise_target != root->frame_id))
    return 8;

  vrt_frame_leave();
  if (vrt_thread_current_frame() != root)
    return 9;

  auto* continuation =
    static_cast<std::jmp_buf*>(vrt_frame_raise_continuation());
  if (continuation == nullptr)
    return 10;

  if (setjmp(*continuation) == 0)
  {
    child = vrt_frame_enter(&child_function);
    if (child == nullptr)
      return 11;

    if (vrt_frame_set_raise_target(root_target) != child->frame_id.raw())
      return 12;

    vrt_frame_raise(42);
  }

  if (
    (vrt_thread_current_frame() != root) ||
    (vrt_frame_take_raised_value() != 42))
    return 13;

  vrt_frame_reuse(nullptr);
  if ((vrt_thread_current_frame() != root) || (vrt_frame_func(root) != nullptr))
    return 14;

  vrt_frame_reuse(&tail_function);
  if (
    (vrt_thread_current_frame() != root) ||
    (vrt_frame_parent(root) != nullptr) || (vrt_frame_id(root) != root_id) ||
    (vrt_frame_func(root) != &tail_function))
    return 15;

  vrt_frame_leave();
  if (vrt_thread_current_frame() != nullptr)
    return 16;

  vrt_thread_deinit();
  return 0;
}