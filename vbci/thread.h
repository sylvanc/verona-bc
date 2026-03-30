#pragma once

#include "frame.h"
#include "header.h"
#include "logging.h"
#include "platform.h"
#include "program.h"
#include "register.h"
#include "stack.h"

#include <functional>
#include <source_location>
#include <type_traits>
#include <unordered_set>
#include <verona.h>

namespace vbci
{
  struct CallbackClosure;

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
    std::vector<const void*> ffi_arg_vals;
#ifndef NDEBUG
    logging::Trace instruction_log;
#endif

  public:
    static ValueTransfer run_async(uint32_t type_id, Function* func);
    static Register& get_register(uint64_t id);
    static Region* frame_region_for_stack(Location stack_loc);

    template<typename... Ts>
    static Register run_sync(Function* func, Ts&&... argv)
    {
      return get().thread_run_sync(func, std::forward<Ts>(argv)...);
    }

    template<typename... Ts>
    static Register run_cleanup(Function* func, Ts&&... argv)
    {
      return get().thread_run_cleanup(func, std::forward<Ts>(argv)...);
    }

    // Called only by the libffi callback trampoline. Marshals C arguments into
    // thread registers, invokes the Verona callback, and writes the C return
    // value. Runtime errors are fatal here because unwinding across libffi is
    // unsafe.
    static void handle_callback(CallbackClosure* cc, void* ret, void** args);

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
    Register thread_run_sync(Function* func, Ts&&... argv)
    {
      assert(args == 0);
      ((arg(args++) = std::forward<Ts>(argv)), ...);
      return thread_run(func);
    }

    template<typename... Ts>
    Register thread_run_cleanup(Function* func, Ts&&... argv)
    {
      try
      {
        return thread_run_sync(func, std::forward<Ts>(argv)...);
      }
      catch (Value& error_value)
      {
        logging::Error log;
        log << error_value.to_string() << std::endl;
        print_error_stack(log, error_value);
        return {};
      }
    }

    void thread_run_behavior(verona::rt::Work* work);
    void thread_handle_callback(CallbackClosure* cc, void* ret, void** args);
    Register thread_run(Function* func);
    void step();
    void pushframe(Function* func, size_t dst);
    void try_pushframe(Function* func, size_t dst);
    void popframe(Register result);
    void raise(Register result, Location target);
    void tailcall(Function* func);
    void teardown(bool tailcall = false);
    void teardown_all();
    void branch(size_t label);
    void check_args(std::vector<uint32_t>& types, bool vararg = false);
    bool try_check_args(std::vector<uint32_t>& types);
    void check_args(std::vector<Field>& fields);
    Register& arg(size_t idx);
    void drop_args();
    void queue_behavior(Register& result, uint32_t type_id, Function* func);

    void print_stack(logging::Log& log, bool top_frame_only = false);
    void print_error_stack(logging::Log& log, const Value& error_value);
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
