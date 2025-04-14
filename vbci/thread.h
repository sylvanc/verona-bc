#pragma once

#include "frame.h"
#include "program.h"

#include <unordered_set>

namespace vbci
{
  struct Thread
  {
    Stack stack;
    std::vector<Value> locals;
    std::unordered_set<Cown*> read;
    std::unordered_set<Cown*> write;
    Cown* result;

    Program* program;
    Frame* frame;

    inline Op opcode(Code code)
    {
      // 8 bit opcode.
      return static_cast<Op>(code & 0xFF);
    }

    inline Local arg0(Code code)
    {
      // 8 bit local index.
      return (code >> 8) & 0xFF;
    }

    inline Local arg1(Code code)
    {
      // 8 bit local index.
      return (code >> 16) & 0xFF;
    }

    inline Local arg2(Code code)
    {
      // 8 bit local index.
      return (code >> 24) & 0xFF;
    }

    void step();
    Value alloc(ClassId class_id, Location loc, Local arg_base);
    void pushframe(Function* func, Local dst, Condition condition);
    void popframe(Value& result, Condition condition);
    void tailcall(Function* func);
    void branch(Local label);
  };
}
