#include "program.h"

#include "failure.h"
#include "object.h"
#include "thread_context.h"
#include "vrt.h"

#include <mutex>
#include <new>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

namespace
{
  std::mutex lifecycle_mutex;
  std::shared_mutex type_registry_mutex;
  bool runtime_initialized = false;
  std::unordered_set<const vrt::Program*> initialized_programs;
  std::unordered_map<uintptr_t, vrt::TypeInfo> type_registry;

  [[maybe_unused]] bool is_valid_value_type(vrt::ValueType value_type)
  {
    switch (value_type)
    {
      case vrt::ValueType::none:
      case vrt::ValueType::scalar:
      case vrt::ValueType::raw_pointer:
      case vrt::ValueType::object:
        return true;

      default:
        return false;
    }
  }

  void init_runtime_services()
  {
    // TODO: Initialize process-wide scheduler and runtime services here.
  }

  void init_memo_slots(const vrt::Program& program)
  {
    (void)program;
    // TODO: Initialize compiler-emitted memo slots in dependency order.
  }

  void run_ffi_initializers(const vrt::Program& program)
  {
    (void)program;
    // TODO: Run compiler-emitted FFI initializers after memo initialization.
  }

  void register_types(const vrt::Program& program)
  {
    internal_check(
      (program.type_count == 0) || (program.types != nullptr),
      vrt::Failure::invalid_program_state);

    std::unique_lock guard(type_registry_mutex);

    try
    {
      for (uintptr_t index = 0; index < program.type_count; index++)
      {
        const auto& type = program.types[index];
        internal_check(
          is_valid_value_type(type.value_type) &&
            ((type.value_type == vrt::ValueType::none) ==
             (type.storage_size == 0)) &&
            (type.element_type_id == 0) &&
            ((index == 0) || (program.types[index - 1].id < type.id)),
          vrt::Failure::invalid_program_state);

        auto [entry, inserted] = type_registry.emplace(type.id, type);
        internal_check(
          inserted ||
            ((entry->second.value_type == type.value_type) &&
             (entry->second.storage_size == type.storage_size) &&
             (entry->second.element_type_id == type.element_type_id)),
          vrt::Failure::invalid_program_state);
      }
    }
    catch (const std::bad_alloc&)
    {
      vrt::fail(vrt::Failure::out_of_memory);
    }
  }

  void init_program_state(const vrt::Program& program)
  {
    internal_check(
      (program.singleton_count == 0) || (program.singletons != nullptr),
      vrt::Failure::invalid_program_state);

    register_types(program);

    for (uintptr_t index = 0; index < program.singleton_count; index++)
    {
      const auto& singleton = program.singletons[index];
      vrt::init_singleton(singleton.storage, singleton.cls);
    }

    init_memo_slots(program);
    run_ffi_initializers(program);
  }
}

vrt::TypeLayout vrt::layout_type_id(uintptr_t type_id)
{
  std::shared_lock guard(type_registry_mutex);
  auto type = type_registry.find(type_id);
  internal_check(
    type != type_registry.end(), Failure::invalid_program_state);

  return {type->second.value_type, type->second.storage_size};
}

extern "C" VRT_EXPORT void vrt_runtime_init(void)
{
  std::lock_guard guard(lifecycle_mutex);
  if (runtime_initialized)
    vrt::fail(vrt::Failure::invalid_runtime_state);

  init_runtime_services();
  runtime_initialized = true;
}

extern "C" VRT_EXPORT void vrt_program_init(const vrt::Program* program)
{
  internal_check(program != nullptr, vrt::Failure::invalid_program_state);

  std::lock_guard guard(lifecycle_mutex);
  if (!runtime_initialized)
    vrt::fail(vrt::Failure::invalid_runtime_state);

  internal_check(
    !initialized_programs.contains(program),
    vrt::Failure::invalid_program_state);

  init_program_state(*program);

  try
  {
    initialized_programs.insert(program);
  }
  catch (const std::bad_alloc&)
  {
    vrt::fail(vrt::Failure::out_of_memory);
  }
}

extern "C" VRT_EXPORT void vrt_invocation_begin(void)
{
  {
    std::lock_guard guard(lifecycle_mutex);
    if (!runtime_initialized || initialized_programs.empty())
      vrt::fail(vrt::Failure::invalid_runtime_state);
  }

  auto& context = vrt::ThreadContext::get();
  if (
    (context.thread.frame != nullptr) || (context.continuation != nullptr) ||
    (context.error_catch_point != nullptr))
    vrt::fail(vrt::Failure::invalid_thread_state);

  vrt::reset_exit_code();
}
