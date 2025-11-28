#pragma once

#include "frame.h"
#include "platform.h"
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
    Function* behavior;
    PC current_pc;
    size_t args;

    std::vector<void*> ffi_arg_addrs;
    std::vector<Value*> ffi_arg_vals;

  public:
    static Value run_async(uint32_t type_id, Function* func);

    template<typename... Ts>
    static void run_sync(Function* func, Ts... argv)
    {
      get().thread_run_sync(func, std::forward<Ts>(argv)...);
    }

    VBCI_KEEP static std::string debug()
    {
      auto& t = get();
      return t.program->debug_info(t.frame->func, t.current_pc);
    }

    static std::pair<Function*, PC> debug_info();

  private:
    Thread();
    static Thread& get();
    static void run_behavior(verona::rt::Work* work);

    template<typename... Ts>
    void thread_run_sync(Function* func, Ts... argv)
    {
      assert(args == 0);
      ((arg(args++) = argv), ...);
      auto ret = thread_run(func);

      if (ret.is_error())
        LOG(Error) << ret.to_string();

      ret.drop();
    }

    void thread_run_behavior(verona::rt::Work* work);
    Value thread_run(Function* func);
    void step();
    void pushframe(Function* func, size_t dst, CallType calltype);
    void popframe(Value& result, Condition condition);
    void tailcall(Function* func);
    void teardown(bool tailcall = false);
    void branch(size_t label);
    void check_args(std::vector<uint32_t>& types, bool vararg = false);
    void check_args(std::vector<Field>& fields);
    Value& arg(size_t idx);
    void drop_args();
    void queue_behavior(Value& result, uint32_t type_id, Function* func);

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
