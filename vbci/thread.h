#pragma once

#include "frame.h"
#include "program.h"
#include "stack.h"

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
    size_t args;

    std::vector<uint64_t> ffi_args;
    std::vector<void*> ffi_arg_addrs;

    Thread() : program(&Program::get()), frame(nullptr), args(0) {}
    static Thread& get();

  public:
    static Value run(Function* func);
    static void run_finalizer(Object* obj);

  private:
    Value thread_run(Function* func);
    void thread_run_finalizer(Object* obj);
    void step();
    void pushframe(Function* func, Local dst, Condition condition);
    void popframe(Value& result, Condition condition);
    void tailcall(Function* func);
    void teardown();
    void branch(Local label);
    void check_args(size_t expect);
  };
}
