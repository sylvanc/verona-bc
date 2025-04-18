#pragma once

#include <vbcc.h>
#include <vbci.h>

namespace vbcc
{
  using namespace vbci;

  struct FuncState
  {
    Node func;
    size_t pcs;
    size_t params;
    std::unordered_map<std::string, uint8_t> label_idxs;
    std::unordered_map<std::string, uint8_t> register_idxs;

    FuncState(Node func) : func(func) {}

    std::optional<uint8_t> get_label_id(Node id);
    bool add_label(Node id);

    std::optional<uint8_t> get_register_id(Node id);
    bool add_register(Node id);
  };

  struct State
  {
    bool error = false;
    Node top;

    std::unordered_map<std::string, Id> func_ids;
    std::unordered_map<std::string, Id> class_ids;
    std::unordered_map<std::string, Id> field_ids;
    std::unordered_map<std::string, Id> method_ids;

    std::vector<Node> primitives;
    std::vector<Node> classes;
    std::vector<FuncState> functions;

    State();

    std::optional<Id> get_class_id(Node id);
    bool add_class(Node cls);

    std::optional<Id> get_field_id(Node id);
    void add_field(Node field);

    std::optional<Id> get_method_id(Node id);
    void add_method(Node method);

    std::optional<Id> get_func_id(Node id);
    FuncState& get_func(Node id);
    FuncState& add_func(Node func);

    void gen();
  };
}
