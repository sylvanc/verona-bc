#pragma once

#include "frame.h"
#include "logging.h"
#include "platform.h"
#include "program.h"
#include "stack.h"
#include "register.h"

#include <source_location>
#include <type_traits>
#include <unordered_set>
#include <verona.h>

namespace vbci
{
  struct Thread
  {
    friend struct Operands;

  private:
    Stack stack;
    std::vector<Frame> frames;
    std::vector<Register> locals;
    std::vector<Object*> finalize;

    Program* program;
    Frame* frame;
    Function* behavior;
    PC current_pc;
    size_t args;

    std::vector<const void*> ffi_arg_addrs;
    std::vector<Register*> ffi_arg_vals;
#ifndef NDEBUG
    logging::Trace instruction_log;
#endif

  public:
    static ValueTransfer run_async(uint32_t type_id, Function* func);
    static Region* frame_local_region(size_t index);
    static bool is_frame_local_region(Region* region);
    static size_t frame_local_index(Region* region);
    static Location region_location(Region* region);

    template<typename... Ts>
    static void run_sync(Function* func, Ts&&... argv)
    {
      get().thread_run_sync(func, std::forward<Ts>(argv)...);
    }

    VBCI_KEEP static std::string debug()
    {
      auto& t = get();
      return t.program->debug_info(t.frame->func, t.current_pc);
    }

    static std::pair<Function*, PC> debug_info();

    // Disable copy semantics
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    // Allow move semantics
    Thread(Thread&&) = delete;
    Thread& operator=(Thread&&) = delete;

  private:
    Thread();
    static Thread& get();
    static void run_behavior(verona::rt::Work* work);

    template<typename... Ts>
    void thread_run_sync(Function* func, Ts&&... argv)
    {
      assert(args == 0);
      ((arg(args++) = std::forward<Ts>(argv)), ...);
      auto ret = thread_run(func);

      if (ret->is_error())
        LOG(Error) << ret->to_string();
    }

    void thread_run_behavior(verona::rt::Work* work);
    Register thread_run(Function* func);
    void step();
    void pushframe(Function* func, size_t dst, CallType calltype);
    void popframe(Register result, Condition condition);
    void tailcall(Function* func);
    void teardown(bool tailcall = false);
    void branch(size_t label);
    void check_args(std::vector<uint32_t>& types, bool vararg = false);
    void check_args(std::vector<Field>& fields);
    Register& arg(size_t idx);
    void drop_args();
    void queue_behavior(Register& result, uint32_t type_id, Function* func);


    void print_stack(logging::Log& log, bool top_frame_only = false);
  #ifndef NDEBUG
    void check_stack_rc_invariant(std::source_location);
  #endif
    void invariant(std::source_location loc = std::source_location::current());

    template<typename... Args>
    SNMALLOC_FAST_PATH void trace_instruction(Args&&... args)
    {
#ifndef NDEBUG
      (instruction_log << ... << std::forward<Args>(args));
#else
      snmalloc::UNUSED(args...);
#endif
    }

    template<typename T = size_t>
    SNMALLOC_FAST_PATH T leb()
    {
      if constexpr (
        (std::is_integral_v<T> && std::is_signed_v<T>) ||
        std::is_floating_point_v<T>)
        return static_cast<T>(program->sleb(frame->pc));
      else
        return static_cast<T>(program->uleb(frame->pc));
    }
  };
}
