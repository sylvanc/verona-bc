#include "program.h"

#include "header.h"
#include "object.h"
#include "vrt.h"

#include <cstddef>
#include <cstdint>
#include <vrt/program.h>
#include <vrt/thread.h>

namespace
{
  constexpr uintptr_t none_type_id = 0x500;
  constexpr uintptr_t scalar_type_id = 0x501;
  constexpr uintptr_t singleton_class_id = 0x502;

  alignas(vrt::Object) std::byte
    singleton_storage[vrt::Object::singleton_storage_size()]{};

  const vrt::Class singleton_class{
    singleton_class_id,
    "ProgramSingleton",
    0,
    1,
    0,
    nullptr,
    0,
    nullptr,
    singleton_storage + vrt::Object::singleton_data_offset()};

  const vrt::TypeInfo types[] = {
    {none_type_id, vrt::ValueType::none, 0, 0},
    {scalar_type_id, vrt::ValueType::scalar, sizeof(uint32_t), 0},
    {singleton_class_id, vrt::ValueType::object, sizeof(void*), 0}};
  const vrt::Singleton singletons[] = {{singleton_storage, &singleton_class}};

  const vrt::Program program{3, types, 1, singletons};
}

int main()
{
  vrt_runtime_init();
  vrt_program_init(&program);

  auto none_layout = vrt::layout_type_id(none_type_id);
  auto scalar_layout = vrt::layout_type_id(scalar_type_id);
  auto object_layout = vrt::layout_type_id(singleton_class_id);
  if (
    (none_layout.value_type != vrt::ValueType::none) ||
    (none_layout.storage_size != 0) ||
    (scalar_layout.value_type != vrt::ValueType::scalar) ||
    (scalar_layout.storage_size != sizeof(uint32_t)) ||
    (object_layout.value_type != vrt::ValueType::object) ||
    (object_layout.storage_size != sizeof(void*)))
    return 1;

  auto* singleton = static_cast<vrt::Object*>(
    vrt::Header::from_data(vrt::ValueType::object, singleton_class.singleton));
  if (
    (singleton->cls != &singleton_class) ||
    (singleton->get_type_id() != singleton_class_id) ||
    (singleton->value_type() != vrt::ValueType::object) ||
    (singleton->location() != vrt::Location::immortal()) ||
    (singleton->fields() != singleton_class.singleton) ||
    (singleton->reference_count != 1) ||
    (singleton->allocation != singleton_storage) || singleton->finalizing)
    return 2;

  vrt_thread_init();
  if (vrt_thread_current() == nullptr)
    return 3;

  set_exit_code(7);
  vrt_invocation_begin();
  if (vrt::get_exit_code() != 0)
    return 4;

  auto* data = static_cast<std::byte*>(singleton_class.singleton);
  *data = std::byte{0x5a};
  set_exit_code(9);
  vrt_invocation_begin();
  if ((vrt::get_exit_code() != 0) || (*data != std::byte{0x5a}))
    return 5;

  vrt_thread_deinit();
  if (vrt_thread_current() != nullptr)
    return 6;

  return 0;
}
