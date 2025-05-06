#pragma once

#include "frame.h"
#include "program.h"
#include "stack.h"

#include <type_traits>
#include <unordered_set>

namespace vbci
{
  struct Thread
  {
  private:
    Stack stack;
    std::vector<Frame> frames;
    std::vector<Value> locals;
    std::vector<Object*> finalize;
    // std::unordered_set<Cown*> read;
    // std::unordered_set<Cown*> write;
    // Cown* result;

    Program* program;
    Frame* frame;
    PC current_pc;
    size_t args;

    std::vector<void*> ffi_arg_addrs;

    Thread() : program(&Program::get()), frame(nullptr), args(0)
    {
      frames.reserve(16);
      locals.resize(1024);
    }

    static Thread& get();

  public:
    static Value run(Function* func);
    static void run_finalizer(Object* obj);

  private:
    Value thread_run(Function* func);
    void thread_run_finalizer(Object* obj);
    void step();
    void pushframe(Function* func, size_t dst, Condition condition);
    void popframe(Value& result, Condition condition);
    void tailcall(Function* func);
    void teardown();
    void branch(size_t label);
    void check_args(std::vector<Id>& types, bool vararg = false);
    void check_args(std::vector<Field>& fields);

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
