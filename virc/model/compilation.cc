#include "compilation.h"

#include "../lang.h"

#include <vbc/format.h>

namespace virc
{
  using namespace vbc;

  void LabelState::resize(size_t size)
  {
    first_def.resize(size);
    first_use.resize(size);
    last_use.resize(size);
    in.resize(size);
    defd.resize(size);
    dead.resize(size);
    out.resize(size);
    used.resize(size);
  }

  std::pair<bool, std::string> LabelState::def(size_t r, Node& node, bool var)
  {
    // Not a var, and has alrady been defined, should not be able to re-define
    if (!var && defd.test(r))
      return {false, "redefinition of register"};

    defd.set(r);

    // Not a var, is in the in set (which means it has been used but not
    // defined) then this is a use before def error
    if (!var && in.test(r))
      return {false, "use before def"};

    if (out.test(r))
    {
      // Assigning to a non-variable used register is an error.
      if (!var)
        return {false, "redefinition of register"};

      automove(r);
    }
    else
    {
      out.set(r);
      dead.reset(r);
    }

    if (!first_def.at(r))
      first_def[r] = node;

    if (!first_use.at(r))
      first_use[r] = node;

    last_use[r] = {};
    return {true, ""};
  }

  bool LabelState::use(size_t r, Node& node)
  {
    // We've used a register. If it's not live, we require it and set it as
    // live.
    if (dead.test(r))
      return false;

    used.set(r);

    if (!out.test(r))
    {
      out.set(r);
      in.set(r);
    }

    if (!first_use.at(r))
      first_use[r] = node;

    last_use[r] = node;
    return true;
  }

  bool LabelState::kill(size_t r)
  {
    // We've killed a register. If it's live, we kill it. If it's not live, we
    // require it.
    if (dead.test(r))
      return false;

    used.set(r);

    if (out.test(r))
      out.reset(r);
    else
      in.set(r);

    dead.set(r);
    last_use[r] = {};
    return true;
  }

  void LabelState::automove(size_t r)
  {
    auto n = last_use.at(r);

    if (!n)
      return;

    last_use[r] = {};
    auto parent = n->parent();

    if ((parent == Arg) && (parent->front() == ArgCopy))
      parent / Type = ArgMove;
    else if (parent == Copy)
      parent->parent()->replace(parent, Move << *parent);
  }

  std::optional<size_t> FuncState::get_label_id(Node id)
  {
    auto index = ST::noemit().string(id->location().view());
    auto find = label_idxs.find(index);

    if (find == label_idxs.end())
      return {};

    return find->second;
  }

  LabelState& FuncState::get_label(Node id)
  {
    auto index = ST::noemit().string(id->location().view());
    auto find = label_idxs.find(index);
    return labels.at(find->second);
  }

  bool FuncState::add_label(Node id)
  {
    auto index = ST::noemit().string(id->location().view());
    auto find = label_idxs.find(index);

    if (find != label_idxs.end())
      return false;

    label_idxs.insert({index, label_idxs.size()});
    labels.emplace_back();
    return true;
  }

  std::optional<size_t> FuncState::get_register_id(Node id)
  {
    auto index = ST::di().string(id);
    auto find = register_idxs.find(index);

    if (find == register_idxs.end())
      return {};

    return find->second;
  }

  bool FuncState::add_register(Node id)
  {
    auto index = ST::di().string(id);
    auto find = register_idxs.find(index);

    if (find != register_idxs.end())
      return false;

    register_idxs.insert({index, register_idxs.size()});
    register_names.push_back(index);
    assert(register_idxs.size() == register_names.size());
    return true;
  }

  Compilation::Compilation()
  {
    primitives.resize(PrimitiveTypeCount);

    // Reserve a function ID for `@main`.
    auto main_name = ST::di().string("@main");
    auto func_main = FuncState(nullptr);
    func_main.name = main_name;
    functions.push_back(func_main);
    func_ids.insert({main_name, MainFuncId});

    // Reserve a method ID for `@final`.
    method_ids.insert({ST::di().string("@final"), FinalMethodId});

    // Reserve a method ID for `@callback`.
    method_ids.insert({ST::di().string("@callback"), CallbackMethodId});
  }

  void Compilation::add_path(const std::filesystem::path& path)
  {
    auto full = std::filesystem::canonical(path);

    if (!std::filesystem::is_directory(full))
      full = full.parent_path();

    source_paths.push_back(full);
  }

  std::optional<size_t> Compilation::get_typealias_id(Node id)
  {
    auto name = ST::di().string(id);
    auto find = type_ids.find(name);

    if (find == type_ids.end())
      return {};

    return find->second;
  }

  Node Compilation::get_typealias(Node id)
  {
    auto name = ST::di().string(id);
    auto find = type_ids.find(name);
    return typealiases.at(find->second);
  }

  bool Compilation::add_typealias(Node type)
  {
    auto name = ST::di().string(type / TypeId);
    auto find = type_ids.find(name);

    if (find != type_ids.end())
      return false;

    type_ids.insert({name, type_ids.size()});
    typealiases.push_back(type);
    return true;
  }

  std::optional<size_t> Compilation::get_class_id(Node id)
  {
    auto name = ST::di().string(id);
    auto find = class_ids.find(name);

    if (find == class_ids.end())
      return {};

    return find->second;
  }

  bool Compilation::add_class(Node cls)
  {
    auto name = ST::di().string(cls / ClassId);
    auto find = class_ids.find(name);

    if (find != class_ids.end())
      return false;

    class_ids.insert({name, class_ids.size()});
    classes.push_back(cls);
    return true;
  }

  std::optional<size_t> Compilation::get_field_id(Node id)
  {
    auto name = ST::di().string(id);
    auto find = field_ids.find(name);

    if (find == field_ids.end())
      return {};

    return find->second;
  }

  void Compilation::add_field(Node field)
  {
    auto name = ST::di().string(field / FieldId);
    auto find = field_ids.find(name);

    if (find == field_ids.end())
      field_ids.insert({name, field_ids.size()});
  }

  std::optional<size_t> Compilation::get_method_id(Node id)
  {
    auto name = ST::di().string(id);
    auto find = method_ids.find(name);

    if (find == method_ids.end())
      return {};

    return find->second;
  }

  void Compilation::add_method(Node method)
  {
    auto name = ST::di().string(method / MethodId);
    auto find = method_ids.find(name);

    if (find == method_ids.end())
      method_ids.insert({name, method_ids.size()});
  }

  std::optional<size_t> Compilation::get_func_id(Node id)
  {
    auto name = ST::di().string(id);
    auto find = func_ids.find(name);

    if (find == func_ids.end())
      return {};

    // Pretend not to have an id if the function name is reserved.
    auto func_id = find->second;

    if (!functions.at(func_id).func)
      return {};

    return func_id;
  }

  FuncState& Compilation::get_func(Node id)
  {
    auto name = ST::di().string(id);
    auto find = func_ids.find(name);
    return functions.at(find->second);
  }

  FuncState& Compilation::add_func(Node func)
  {
    auto name = ST::di().string(func / FunctionId);
    auto find = func_ids.find(name);
    size_t func_id;

    if (find == func_ids.end())
    {
      // This is a fresh func_id.
      func_id = func_ids.size();
      func_ids.insert({name, func_id});
      functions.push_back(func);
    }
    else
    {
      // This is a reserved func_id.
      func_id = find->second;
      functions.at(func_id).func = func;
    }

    auto& func_state = functions.at(func_id);
    func_state.name = name;
    func_state.params = (func / Params)->size();
    return func_state;
  }

  std::optional<size_t> Compilation::get_symbol_id(Node id)
  {
    auto name = ST::noemit().string(id);
    auto find = symbol_ids.find(name);

    if (find == symbol_ids.end())
      return {};

    return find->second;
  }

  Node Compilation::get_symbol(Node id)
  {
    auto name = ST::noemit().string(id);
    auto find = symbol_ids.find(name);

    if (find == symbol_ids.end())
      return {};

    return symbols[find->second];
  }

  bool Compilation::add_symbol(Node symbol)
  {
    auto name = ST::noemit().string(symbol / SymbolId);
    auto find = symbol_ids.find(name);

    if (find != symbol_ids.end())
      return false;

    ST::exec().string(symbol / Lhs);
    ST::exec().string(symbol / Rhs);
    symbol_ids.insert({name, symbol_ids.size()});
    symbols.push_back(symbol);
    return true;
  }

  std::optional<size_t> Compilation::get_library_id(Node lib)
  {
    auto name = ST::exec().string(lib / String);
    auto find = library_ids.find(name);

    if (find == library_ids.end())
      return {};

    return find->second;
  }

  void Compilation::add_library(Node lib)
  {
    auto name = ST::exec().string(lib / String);
    auto find = library_ids.find(name);

    if (find != library_ids.end())
      return;

    library_ids.insert({name, library_ids.size()});
    libraries.push_back(lib);
  }

  size_t Compilation::type_id(Node type)
  {
    while (type == TypeId)
      type = get_typealias(type) / Type;

    if (type == Dyn)
    {
      return DynId;
    }
    else if (type->in(
               {None,
                Bool,
                I8,
                U8,
                I16,
                U16,
                I32,
                U32,
                I64,
                U64,
                ILong,
                ULong,
                ISize,
                USize,
                F32,
                F64,
                Ptr}))
    {
      return +val(type);
    }
    else if (type == ClassId)
    {
      return *get_class_id(type) + PrimitiveTypeCount;
    }

    TypeInfo info;

    if (type == Array)
    {
      info = {TypeKind::Array, {type_id(type / Type)}};
    }
    else if (type == Cown)
    {
      info = {TypeKind::Cown, {type_id(type / Type)}};
    }
    else if (type == Ref)
    {
      info = {TypeKind::Ref, {type_id(type / Type)}};
    }
    else if (type == Union)
    {
      for (auto& child : *type)
        info.elements.push_back(type_id(child));

      info.kind = TypeKind::Union;
      std::sort(info.elements.begin(), info.elements.end());
      info.elements.erase(
        std::unique(info.elements.begin(), info.elements.end()),
        info.elements.end());
    }
    else if (type == TupleType)
    {
      info.kind = TypeKind::Tuple;

      for (auto& child : *type)
        info.elements.push_back(type_id(child));
    }
    else
    {
      assert(false);
    }

    auto find = type_info_ids.find(info);
    if (find != type_info_ids.end())
      return find->second;

    auto id = types.size() + classes.size() + PrimitiveTypeCount;
    type_info_ids.emplace(info, id);
    types.push_back(std::move(info));
    return id;
  }

}
