#pragma once

#include "frame.h"
#include "program.h"
#include "stack.h"

#include <type_traits>
#include <unordered_set>
#include <verona.h>

namespace vbci
{
  struct Thread
  {
  private:
    Stack stack;
    std::vector<Frame> frames;
    std::vector<Value> locals;
    std::vector<Object*> finalize;

    Program* program;
    Frame* frame;
    PC current_pc;
    size_t args;

    std::vector<void*> ffi_arg_addrs;
    std::vector<Value*> ffi_arg_vals;

  public:
    static Value run_async(Function* func);

    template<typename... Ts>
    static Value run_sync(Function* func, Ts... argv)
    {
      return get().thread_run_sync(func, std::forward<Ts>(argv)...);
    }

  private:
    Thread();
    static Thread& get();
    static void run_behavior(verona::rt::Work* work);

    template <typename... Ts>
    Value thread_run_sync(Function* func, Ts... argv)
    {
      assert(args == 0);
      ((arg(args++) = argv), ...);
      return get().thread_run(func);
    }

    void thread_run_behavior(verona::rt::Work* work);
    Value thread_run(Function* func);
    void step();
    void pushframe(Function* func, size_t dst, Condition condition);
    void popframe(Value& result, Condition condition);
    void tailcall(Function* func);
    void teardown();
    void branch(size_t label);
    void check_args(std::vector<Id>& types, bool vararg = false);
    void check_args(std::vector<Field>& fields);
    Value& arg(size_t idx);
    void drop_args();

    template<typename T = size_t>
    T leb()
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
