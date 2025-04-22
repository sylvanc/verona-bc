#pragma once

#include "frame.h"
#include "function.h"
#include "ident.h"
#include "logging.h"
#include "types.h"
#include "value.h"

#include <bit>
#include <filesystem>
#include <fstream>
#include <vector>

namespace vbci
{
  struct Program
  {
    std::filesystem::path file;
    std::vector<Code> code;
    std::vector<uint8_t> di;

    std::vector<Function> functions;
    std::vector<Class> primitives;
    std::vector<Class> classes;
    std::vector<Value> globals;

    static Program& get()
    {
      static Program program;
      return program;
    }

    Function* get_function(Id id)
    {
      if (id >= functions.size())
        throw Value(Error::UnknownFunction);

      return &functions.at(id);
    }

    Code load_code(PC& pc)
    {
      return code.at(pc++);
    }

    PC load_pc(PC& pc)
    {
      return load_u64(pc);
    }

    int16_t load_i16(PC& pc)
    {
      return code.at(pc++);
    }

    int32_t load_i32(PC& pc)
    {
      return code.at(pc++);
    }

    int64_t load_i64(PC& pc)
    {
      auto lo = code.at(pc++);
      auto hi = code.at(pc++);
      return (int64_t(hi) << 32) | int64_t(lo);
    }

    uint16_t load_u16(PC& pc)
    {
      return code.at(pc++);
    }

    uint32_t load_u32(PC& pc)
    {
      return code.at(pc++);
    }

    uint64_t load_u64(PC& pc)
    {
      auto lo = code.at(pc++);
      auto hi = code.at(pc++);
      return (uint64_t(hi) << 32) | uint64_t(lo);
    }

    float load_f32(PC& pc)
    {
      return std::bit_cast<float>(code.at(pc++));
    }

    double load_f64(PC& pc)
    {
      auto lo = code.at(pc++);
      auto hi = code.at(pc++);
      return std::bit_cast<double>((static_cast<uint64_t>(hi) << 32) | lo);
    }

    bool load(std::filesystem::path& path);
    bool parse();
    bool parse_function(Function& f, PC& pc);
    bool parse_fields(Class& cls, PC& pc);
    bool parse_methods(Class& cls, PC& pc);
    int run();
  };
}
