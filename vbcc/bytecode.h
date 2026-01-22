#pragma once

#include "bitset.h"
#include "stringtable.h"

#include <vbcc.h>
#include <vbci.h>

namespace vbcc
{
  using namespace vbci;

  struct VecHash
  {
    size_t operator()(const std::vector<uint8_t>& v) const noexcept;
  };

  struct LabelState
  {
    std::vector<size_t> pred;
    std::vector<size_t> succ;
    std::vector<Node> first_def;
    std::vector<Node> first_use;
    std::vector<Node> last_use;

    Bitset in;
    Bitset defd;
    Bitset dead;
    Bitset out;

    void resize(size_t size);
    std::pair<bool, std::string> def(size_t r, Node& node, bool var);
    bool use(size_t r, Node& node);
    bool kill(size_t r);
    void automove(size_t r);
  };

  struct FuncState
  {
    ST::Index name;
    Node func;
    size_t params;
    size_t label_pcs;

    std::unordered_map<ST::Index, size_t> label_idxs;
    std::unordered_map<ST::Index, size_t> register_idxs;
    std::vector<ST::Index> register_names;
    std::vector<LabelState> labels;

    FuncState(Node func) : func(func) {}

    std::optional<size_t> get_label_id(Node id);
    std::optional<std::reference_wrapper<LabelState>> get_label(Node id);
    bool add_label(Node id);

    std::optional<size_t> get_register_id(Node id);
    bool add_register(Node id);
  };

  struct Bytecode
  {
    std::vector<std::filesystem::path> source_paths;
    bool error = false;
    Node top;

    std::unordered_map<ST::Index, size_t> type_ids;
    std::unordered_map<ST::Index, size_t> class_ids;
    std::unordered_map<ST::Index, size_t> field_ids;
    std::unordered_map<ST::Index, size_t> method_ids;
    std::unordered_map<ST::Index, size_t> func_ids;
    std::unordered_map<ST::Index, size_t> symbol_ids;
    std::unordered_map<ST::Index, size_t> library_ids;

    std::vector<Node> typealiases;
    std::vector<Node> primitives;
    std::vector<Node> complex_primitives;
    std::vector<Node> classes;
    std::vector<FuncState> functions;
    std::vector<Node> symbols;
    std::vector<Node> libraries;

    std::unordered_map<std::vector<uint8_t>, size_t, VecHash> type_map;
    std::vector<std::vector<uint8_t>> types;

    Bytecode();

    void add_path(const std::filesystem::path& path);

    std::optional<size_t> get_typealias_id(Node id);
    Node get_typealias(Node id);
    bool add_typealias(Node type);

    std::optional<size_t> get_class_id(Node id);
    bool add_class(Node cls);

    std::optional<size_t> get_field_id(Node id);
    void add_field(Node field);

    std::optional<size_t> get_method_id(Node id);
    void add_method(Node method);

    std::optional<size_t> get_func_id(Node id);
    std::optional<std::reference_wrapper<FuncState>> get_func(Node id);
    FuncState& add_func(Node func);

    std::optional<size_t> get_symbol_id(Node id);
    bool add_symbol(Node symbol);

    std::optional<size_t> get_library_id(Node id);
    void add_library(Node lib);

    void gen(std::filesystem::path output, bool strip);
    size_t typ(Node type);
  };
}
