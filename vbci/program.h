#pragma once

#include "dynlib.h"
#include "frame.h"
#include "function.h"
#include "ident.h"
#include "logging.h"
#include "types.h"
#include "value.h"

#include <bit>
#include <fstream>
#include <vector>

namespace vbci
{
  struct SourceFile
  {
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
    Code* code;

    std::vector<Typedef> typedefs;
    std::vector<Function> functions;
    std::vector<Class> primitives;
    std::vector<Class> classes;
    std::vector<Value> globals;

    std::vector<Dynlib> libs;
    std::vector<Symbol> symbols;

    PC di;
    size_t di_compilation_path;
    std::vector<std::string> di_strings;
    std::unordered_map<std::string, SourceFile> source_files;

  public:
    static Program& get();

    Symbol& symbol(Id id);
    Function* function(Id id);
    Class& primitive(Id id);
    Class& cls(Id id);
    Value& global(Id id);

    Code load_code(PC& pc);
    PC load_pc(PC& pc);
    int16_t load_i16(PC& pc);
    int32_t load_i32(PC& pc);
    int64_t load_i64(PC& pc);
    uint16_t load_u16(PC& pc);
    uint32_t load_u32(PC& pc);
    uint64_t load_u64(PC& pc);
    float load_f32(PC& pc);
    double load_f64(PC& pc);

    int run(std::filesystem::path& path);

    bool typecheck(Id t1, Id t2);

    std::string debug_info(Function* func, PC pc);
    std::string di_function(Function* func);
    std::string di_class(Class* cls);
    std::string di_field(Class* cls, FieldIdx idx);

  private:
    bool load();
    bool parse_function(Function& f, PC& pc);
    bool parse_fields(Class& cls, PC& pc);
    bool parse_methods(Class& cls, PC& pc);

    size_t uleb(size_t& pc);
    std::string str(size_t& pc);
    void string_table(size_t& pc, std::vector<std::string>& table);

    SourceFile* get_source_file(const std::string& path);
  };
}
