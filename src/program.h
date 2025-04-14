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

    // Lookup FuncId as an index into this vector.
    // FuncId indexes are unique to name plus arity.
    std::vector<Function> functions;

    // Lookup ValueType as an index into this vector.
    std::vector<Class> primitives;

    // Lookup ClassId as an index into this vector.
    std::vector<Class> classes;

    // Lookup GlobalId as an index into this vector.
    std::vector<Value> globals;

    static Program& get()
    {
      static Program program;
      return program;
    }

    bool load(std::filesystem::path& path);
    bool parse();
    bool parse_class(Class& cls, PC& pc);

    Function* get_function(FuncId id)
    {
      if (id >= functions.size())
        throw Value(Error::UnknownFunction);

      return &functions.at(id);
    }

    Code load_code(PC& pc)
    {
      return code[pc++];
    }

    int16_t load_i16(PC& pc)
    {
      return code[pc++];
    }

    int32_t load_i32(PC& pc)
    {
      return code[pc++];
    }

    int64_t load_i64(PC& pc)
    {
      auto lo = code[pc++];
      auto hi = code[pc++];
      return (static_cast<int64_t>(hi) << 32) | lo;
    }

    uint16_t load_u16(PC& pc)
    {
      return code[pc++];
    }

    uint32_t load_u32(PC& pc)
    {
      return code[pc++];
    }

    uint64_t load_u64(PC& pc)
    {
      auto lo = code[pc++];
      auto hi = code[pc++];
      return (static_cast<uint64_t>(hi) << 32) | lo;
    }

    float load_f32(PC& pc)
    {
      return std::bit_cast<float>(code[pc++]);
    }

    double load_f64(PC& pc)
    {
      auto lo = code[pc++];
      auto hi = code[pc++];
      return std::bit_cast<double>((static_cast<uint64_t>(hi) << 32) | lo);
    }
  };
}
