#pragma once

#include "frame.h"
#include "program.h"
#include "stack.h"

#include <bitset>
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
    std::bitset<MaxRegisters> args;
    // std::unordered_set<Cown*> read;
    // std::unordered_set<Cown*> write;
    // Cown* result;

    Program* program;
    Frame* frame;

    Thread() : program(&Program::get()), frame(nullptr) {}
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
