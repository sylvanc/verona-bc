#include "program.h"
#include "vrt.h"

#include <cstdint>
#include <vrt/program.h>
#include <vrt/thread.h>

namespace
{
  constexpr uintptr_t none_type_id = 0x500;
  constexpr uintptr_t scalar_type_id = 0x501;
  const vrt::TypeInfo types[] = {
    {none_type_id, vrt::ValueType::none, 0, 0},
    {scalar_type_id, vrt::ValueType::scalar, sizeof(uint32_t), 0}};
  const vrt::Program program{2, types, 0, nullptr};
}

int main()
{
  vrt_runtime_init();
  vrt_program_init(&program);

  auto none_layout = vrt::layout_type_id(none_type_id);
  auto scalar_layout = vrt::layout_type_id(scalar_type_id);
  if (
    (none_layout.value_type != vrt::ValueType::none) ||
    (none_layout.storage_size != 0) ||
    (scalar_layout.value_type != vrt::ValueType::scalar) ||
    (scalar_layout.storage_size != sizeof(uint32_t)))
    return 1;

  vrt_thread_init();

  set_exit_code(7);
  vrt_invocation_begin();
  if (vrt::get_exit_code() != 0)
    return 2;

  set_exit_code(9);
  vrt_invocation_begin();
  if (vrt::get_exit_code() != 0)
    return 3;

  vrt_thread_deinit();
  return 0;
}
