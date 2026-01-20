#pragma once

#include "classes.h"
#include "dynlib.h"
#include "frame.h"
#include "function.h"
#include "ident.h"
#include "logging.h"
#include "value.h"

#include <bit>
#include <fstream>
#include <vector>

namespace vbci
{
  struct SourceFile
  {
    size_t di_pos;
    std::string contents;
    std::vector<size_t> lines;

    std::pair<size_t, size_t> linecol(size_t pos);
    std::string line(size_t line);
  };

  struct Program
  {
  private:
    std::filesystem::path file;
    std::vector<uint8_t> content;
    std::vector<std::string> strings;

    std::vector<Function> functions;
    std::vector<Class> classes;
    std::vector<ComplexType> complex_types;
    std::vector<Value> globals;
    std::unordered_map<uint32_t, uint32_t> ref_map;

    std::vector<Dynlib> libs;
    std::vector<Symbol> symbols;

    ffi_type ffi_type_value;
    std::vector<ffi_type*> ffi_type_value_elements;

    uint32_t min_complex_type_id;
    uint32_t typeid_cown_i32;
    uint32_t typeid_arg;
    uint32_t typeid_argv;
    uint32_t typeid_ref_dyn;
    Array* argv = nullptr;

    PC di = PC(-1);
    std::vector<uint8_t> di_content;
    std::vector<std::string> di_strings;
    std::unordered_map<size_t, SourceFile> source_files;

  public:
    static Program& get();

    // Disable copy constructors
    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;

    // Default constructor
    Program() = default;

    // Move constructors
    Program(Program&&) = default;
    Program& operator=(Program&&) = default;

    Symbol& symbol(size_t idx);
    Function* function(size_t idx);
    Class& cls(uint32_t type_id);
    ComplexType& complex_type(uint32_t type_id);
    Value& global(size_t idx);
    ffi_type* value_type();

    SNMALLOC_FAST_PATH int64_t sleb(size_t& pc)
    {
      // This uses zigzag encoding.
      auto value = uleb(pc);
      return (value >> 1) ^ -(value & 1);
    }

    SNMALLOC_FAST_PATH uint64_t uleb(size_t& pc)
    {
      return uleb(pc, content);
    }

    SNMALLOC_FAST_PATH uint64_t di_uleb(size_t& pc)
    {
      return uleb(pc, di_content);
    }

    SNMALLOC_FAST_PATH uint64_t uleb(size_t& pc, std::vector<uint8_t>& from)
    {
      constexpr uint64_t max_shift = (sizeof(uint64_t) * 8) - 1;
      uint64_t value = 0;

      for (uint64_t shift = 0; shift <= max_shift; shift += 7)
      {
        value |= (uint64_t(from.at(pc)) & 0x7F) << shift;
        if (SNMALLOC_LIKELY((from.at(pc++) & 0x80) == 0)) [[likely]]
          break;
      }

      return value;
    }

    uint32_t get_typeid_arg();
    uint32_t get_typeid_argv();
    Array* get_argv();
    Array* get_string(size_t idx);

    int run(
      std::filesystem::path& path,
      size_t num_threads,
      std::vector<std::string> args);

    std::pair<ValueType, ffi_type*> layout_type_id(uint32_t type_id);
    std::pair<ValueType, ffi_type*> layout_union_type(ComplexType& t);

    bool is_complex(uint32_t type_id);
    bool is_array(uint32_t type_id);
    bool is_ref(uint32_t type_id);
    bool is_cown(uint32_t type_id);
    bool is_union(uint32_t type_id);
    uint32_t unarray(uint32_t type_id);
    uint32_t uncown(uint32_t type_id);
    uint32_t unref(uint32_t type_id);
    uint32_t ref(uint32_t type_id);
    bool subtype(uint32_t sub, uint32_t super);

    std::string debug_info(Function* func, PC pc);
    std::string di_function(Function* func);
    std::string di_class(Class& cls);
    std::string di_field(Class& cls, size_t idx);

  private:
    void setup_value_type();
    void setup_argv(std::vector<std::string>& args);
    bool load();
    bool parse_function(Function& f, PC& pc);
    bool parse_fields(Class& cls, PC& pc);
    bool parse_methods(Class& cls, PC& pc);
    bool fixup_methods(Class& cls);
    void parse_complex_type(ComplexType& t, uint32_t type_id, PC& pc);

    std::string str(size_t& pc, std::vector<uint8_t>& from);
    void string_table(
      size_t& pc, std::vector<uint8_t>& from, std::vector<std::string>& table);

    bool di_decompress();
    SourceFile* get_source_file(size_t di_file);
  };
}
