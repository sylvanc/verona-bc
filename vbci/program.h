#pragma once

#include "classes.h"
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
    std::vector<std::string> strings;

    std::vector<Function> functions;
    std::vector<Class> primitives;
    std::vector<Class> classes;
    std::vector<ComplexType> complex_types;
    std::vector<Value> globals;

    std::vector<std::shared_ptr<Dynlib>> libs;
    std::vector<Symbol> symbols;

    ffi_type ffi_type_value;
    std::vector<ffi_type*> ffi_type_value_elements;

    Array* argv = nullptr;

    PC di = PC(-1);
    size_t di_compilation_path = 0;
    std::vector<std::string> di_strings;
    std::unordered_map<std::string, SourceFile> source_files;

  public:
    static Program& get();

    Symbol& symbol(size_t idx);
    Function* function(size_t idx);
    Class& primitive(size_t idx);
    Class& cls(TypeId type_id);
    Value& global(size_t idx);
    ComplexType& complex_type(size_t idx);
    ffi_type* value_type();

    int64_t sleb(size_t& pc);
    uint64_t uleb(size_t& pc);

    Array* get_argv();
    Array* get_string(size_t idx);

    int run(
      std::filesystem::path& path,
      size_t num_threads,
      std::vector<std::string> args);

    std::pair<ValueType, ffi_type*> layout_type_id(TypeId type_id);
    std::pair<ValueType, ffi_type*> layout_complex_type(ComplexType& t);

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
    bool parse_complex_type(ComplexType*& t, PC& pc);

    std::string str(size_t& pc);
    void string_table(size_t& pc, std::vector<std::string>& table);

    SourceFile* get_source_file(const std::string& path);
  };
}
