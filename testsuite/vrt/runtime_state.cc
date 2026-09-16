#include "frame.h"
#include "thread.h"
#include "vrt.h"

#include <csetjmp>
#include <thread>
#include <type_traits>

static_assert(sizeof(vrt::Location) == sizeof(uintptr_t));
static_assert(std::is_trivially_copyable_v<vrt::Location>);
static_assert(!std::is_default_constructible_v<vrt::Location>);
static_assert(
  std::is_same_v<decltype(vrt::Frame::raise_target), vrt::Location>);
static_assert(std::is_same_v<decltype(vrt::Frame::frame_id), vrt::Location>);
static_assert(std::is_same_v<decltype(vrt::Thread::frame), vrt::Frame*>);
static_assert(sizeof(vrt::Thread) == sizeof(vrt::Frame*));

namespace
{
  void test_entry() {}
}

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

  auto root_location = vrt::Location::stack();
  auto child_location = root_location.next_stack_level();
  if (
    (root_location.raw() != 0x1) || (child_location.raw() != 0x9) ||
    !root_location.is_stack() ||
    (root_location.stack_index() != 0) ||
    (child_location.stack_index() != 1) ||
    !(root_location < child_location) || !(root_location <= child_location) ||
    !(child_location > root_location) || !(child_location >= root_location) ||
    (root_location == child_location) ||
    (vrt::Location::from_raw(child_location.raw()) != child_location))
    return 27;

  const vrt_func root_function{1, "root", test_entry};
  const vrt_func child_function{2, "child", test_entry};
  const vrt_func tail_function{3, "tail", test_entry};

  if (vrt_func_get_ptr(&root_function) != test_entry)
    return 26;

  if (
    (vrt_thread_current() != nullptr) ||
    (vrt_thread_current_frame() != nullptr) ||
    (vrt_frame_parent(nullptr) != nullptr) || (vrt_frame_id(nullptr) != 0) ||
    (vrt_frame_func(nullptr) != nullptr))
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
    (vrt_frame_func(root) != &root_function) || (root->region == nullptr) ||
    (root->frame_id != root_location) ||
    (root->raise_target != root->frame_id))
    return 7;

  const auto root_id = vrt_frame_id(root);
  const auto root_target = root->frame_id.raw();
  if (
    (root_id == 0) || (root_id != root_target) ||
    (vrt_frame_get_raise_target() != root_target))
    return 8;

  const auto temporary_target = root_target + 1000;
  if (
    (vrt_frame_set_raise_target(temporary_target) != root_target) ||
    (vrt_frame_get_raise_target() != temporary_target) ||
    (vrt_frame_set_raise_target(root_target) != temporary_target) ||
    (vrt_frame_get_raise_target() != root_target))
    return 23;

  auto* child = vrt_frame_enter(&child_function);
  if (
    (child == nullptr) || (vrt_thread_current_frame() != child) ||
    (vrt_frame_parent(child) != root) ||
    (vrt_frame_func(child) != &child_function) ||
    (vrt_frame_id(child) == root_id) || (child->region == nullptr) ||
    (child->frame_id != child_location) ||
    (child->raise_target != child->frame_id))
    return 9;

  const auto child_id = vrt_frame_id(child);
  const auto child_target = child->frame_id.raw();
  if (
    (child_id != child_target) ||
    (vrt_frame_get_raise_target() != child_target) ||
    (vrt_frame_set_raise_target(root_target) != child_target) ||
    (vrt_frame_get_raise_target() != root_target))
    return 24;

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

    if (vrt_frame_set_raise_target(root_target) != child->frame_id.raw())
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
    (vrt_frame_func(root) != nullptr))
    return 13;

  vrt_frame_reuse(&tail_function);
  if (
    (vrt_thread_current_frame() != root) || (vrt_frame_parent(root) != nullptr) ||
    (vrt_frame_id(root) != root_id) ||
    (vrt_frame_func(root) != &tail_function))
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
