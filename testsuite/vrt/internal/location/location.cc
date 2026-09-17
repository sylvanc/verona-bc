#include "frame.h"
#include "location.h"
#include "thread.h"

#include <cstdint>
#include <type_traits>

static_assert(sizeof(vrt::Location) == sizeof(uintptr_t));
static_assert(std::is_trivially_copyable_v<vrt::Location>);
static_assert(!std::is_default_constructible_v<vrt::Location>);
static_assert(
  std::is_same_v<decltype(vrt::Frame::raise_target), vrt::Location>);
static_assert(std::is_same_v<decltype(vrt::Frame::frame_id), vrt::Location>);
static_assert(std::is_same_v<decltype(vrt::Thread::frame), vrt::Frame*>);
static_assert(sizeof(vrt::Thread) == sizeof(vrt::Frame*));

int main()
{
  auto root = vrt::Location::stack();
  auto child = root.next_stack_level();
  if (
    (root.raw() != 0x1) || (child.raw() != 0x9) || !root.is_stack() ||
    (root.stack_index() != 0) || (child.stack_index() != 1) ||
    !(root < child) || !(root <= child) || !(child > root) ||
    !(child >= root) || (root == child) ||
    (vrt::Location::from_raw(child.raw()) != child))
    return 1;

  return 0;
}
