#include "lang.h"
#include "vbci.h"
#include "wf.h"

namespace vbcc
{
  PassDef bytecode()
  {
    using namespace vbci;

    struct FuncState
    {
      Node func;
      std::unordered_map<std::string, uint8_t> label_idxs;
      std::unordered_map<std::string, uint8_t> register_idxs;

      FuncState(Node func) : func(func) {}

      std::optional<uint8_t> get_label_id(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = label_idxs.find(name);

        if (find == label_idxs.end())
          return {};

        return find->second;
      }

      bool add_label(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = label_idxs.find(name);

        if (find != label_idxs.end())
          return false;

        label_idxs.insert({name, label_idxs.size()});
        return true;
      }

      std::optional<uint8_t> get_register_id(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = register_idxs.find(name);

        if (find == register_idxs.end())
          return {};

        return find->second;
      }

      bool add_register(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = register_idxs.find(name);

        if (find != register_idxs.end())
          return false;

        register_idxs.insert({name, register_idxs.size()});
        return true;
      }
    };

    struct State
    {
      bool error = false;

      std::unordered_map<std::string, FuncId> func_ids;
      std::unordered_map<std::string, ClassId> class_ids;
      std::unordered_map<std::string, FieldId> field_ids;
      std::unordered_map<std::string, MethodId> method_ids;

      std::vector<Node> primitives;
      std::vector<Node> classes;
      std::vector<FuncState> functions;

      std::optional<ClassId> get_class_id(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = class_ids.find(name);

        if (find == class_ids.end())
          return {};

        return find->second;
      }

      bool add_class(Node cls)
      {
        auto name = std::string((cls / GlobalId)->location().view());
        auto find = class_ids.find(name);

        if (find != class_ids.end())
          return false;

        class_ids.insert({name, class_ids.size()});
        classes.push_back(cls);
        return true;
      }

      std::optional<FieldId> get_field_id(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = field_ids.find(name);

        if (find == field_ids.end())
          return {};

        return find->second;
      }

      void add_field(Node field)
      {
        auto name = std::string((field / GlobalId)->location().view());
        auto find = field_ids.find(name);

        if (find == field_ids.end())
          field_ids.insert({name, field_ids.size()});
      }

      std::optional<MethodId> get_method_id(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = method_ids.find(name);

        if (find == method_ids.end())
          return {};

        return find->second;
      }

      void add_method(Node method)
      {
        auto name = std::string((method / Lhs)->location().view());
        auto find = method_ids.find(name);

        if (find == method_ids.end())
          method_ids.insert({name, method_ids.size()});
      }

      std::optional<FuncId> get_func_id(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = func_ids.find(name);

        if (find == func_ids.end())
          return {};

        return find->second;
      }

      FuncState& add_func(Node func)
      {
        auto name = std::string((func / GlobalId)->location().view());
        func_ids.insert({name, func_ids.size()});
        functions.push_back(func);
        return functions.back();
      }
    };

    auto state = std::make_shared<State>();
    state->primitives.resize(NumPrimitiveClasses);

    PassDef p{
      "bytecode",
      wfPassLabels,
      dir::topdown | dir::once,
      {
        // Accumulate primitive classes.
        T(Primitive)[Primitive] >> [state](Match& _) -> Node {
          auto primitive = _(Primitive);
          auto ptype = primitive / Type / Type;
          ValueType vtype;

          if (ptype == None)
            vtype = ValueType::None;
          else if (ptype == Bool)
            vtype = ValueType::Bool;
          else if (ptype == I8)
            vtype = ValueType::I8;
          else if (ptype == I16)
            vtype = ValueType::I16;
          else if (ptype == I32)
            vtype = ValueType::I32;
          else if (ptype == I64)
            vtype = ValueType::I64;
          else if (ptype == U8)
            vtype = ValueType::U8;
          else if (ptype == U16)
            vtype = ValueType::U16;
          else if (ptype == U32)
            vtype = ValueType::U32;
          else if (ptype == U64)
            vtype = ValueType::U64;
          else if (ptype == F32)
            vtype = ValueType::F32;
          else if (ptype == F64)
            vtype = ValueType::F64;
          else
          {
            state->error = true;
            return err(ptype, "unknown primitive type");
          }

          auto idx = static_cast<size_t>(vtype);
          if (state->primitives.at(idx))
          {
            state->error = true;
            return err(ptype, "duplicate primitive class");
          }

          state->primitives[idx] = primitive;
          return NoChange;
        },

        // Accumulate user-defined classes.
        T(Class)[Class] >> [state](Match& _) -> Node {
          if (!state->add_class(_(Class)))
          {
            state->error = true;
            return err(_(Class) / GlobalId, "duplicate class name");
          }
          return NoChange;
        },

        // Accumulate field names.
        T(Field)[Field] >> [state](Match& _) -> Node {
          state->add_field(_(Field));
          return NoChange;
        },

        // Accumulate method names.
        T(Method)[Method] >> [state](Match& _) -> Node {
          auto method = _(Method);
          state->add_method(method);

          if (!state->get_func_id(method / Rhs))
          {
            state->error = true;
            return err(method / Rhs, "unknown function");
          }

          return NoChange;
        },

        // Accumulate functions.
        T(Func)[Func] >> [state](Match& _) -> Node {
          auto func = _(Func);

          if (state->get_func_id(func / GlobalId))
          {
            state->error = true;
            return err(func / GlobalId, "duplicate function name");
          }

          auto& func_state = state->add_func(func);

          if ((func / Labels)->size() == 0)
          {
            state->error = true;
            return err(func / GlobalId, "function has no labels");
          }

          if ((func / Labels)->size() >= MaxRegisters)
          {
            state->error = true;
            return err(func / GlobalId, "function has too many labels");
          }

          if ((func / Params)->size() >= MaxRegisters)
          {
            state->error = true;
            return err(func / GlobalId, "function has too many params");
          }

          // Register label names.
          for (auto label : *(func / Labels))
          {
            if (!func_state.add_label(label / LabelId))
            {
              state->error = true;
              return err(label / LabelId, "duplicate label name");
            }
          }

          // Register parameter names.
          for (auto param : *(func / Params))
          {
            if (!func_state.add_register(param / LocalId))
            {
              state->error = true;
              return err(param / LocalId, "duplicate parameter name");
            }
          }

          return NoChange;
        },
      }};

    p.post([state](auto) {
      if (state->error)
        return 0;

      std::vector<Code> code;
      code.push_back(MagicNumber);
      code.push_back(CurrentVersion);

      // Primitive classes.
      for (auto& p : state->primitives)
      {
        if (p)
        {
          auto methods = p / Methods;
          code.push_back(methods->size());

          for (auto& method : *methods)
          {
            code.push_back(state->get_method_id(method / Lhs).value());
            code.push_back(state->get_func_id(method / Rhs).value());
          }
        }
        else
        {
          code.push_back(0);
        }
      }

      // Classes.
      for (auto& c : state->classes)
      {
        auto fields = c / Fields;
        code.push_back(fields->size());

        for (auto& field : *fields)
          code.push_back(state->get_field_id(field / GlobalId).value());

        auto methods = c / Methods;
        code.push_back(methods->size());

        for (auto& method : *methods)
        {
          code.push_back(state->get_method_id(method / Lhs).value());
          code.push_back(state->get_func_id(method / Rhs).value());
        }
      }

      // Functions.
      code.push_back(state->functions.size());

      for (auto& func_state : state->functions)
      {
        // 8 bit label count, 8 bit param count.
        code.push_back(
          (((func_state.func / Params)->size() << 8) |
           (func_state.func / Labels)->size()));

        // TODO: 64 bit PC for each label.

        for (auto label: *(func_state.func / Labels))
        {
          auto pc = code.size();
          (void)pc;

          for (auto stmt: *(label / Body))
          {
            // TODO:
          }

          auto term = label / Return;
          (void)term;
        }
      }

      if constexpr (std::endian::native == std::endian::big)
      {
        for (size_t i = 0; i < code.size(); i++)
          code[i] = std::byteswap(code[i]);
      }

      std::ofstream f(
        options().bytecode_file, std::ios::binary | std::ios::out);

      if (f)
      {
        auto data = reinterpret_cast<const char*>(code.data());
        auto size = code.size() * sizeof(Code);
        f.write(data, size);
      }

      if (!f)
      {
        logging::Error() << "Error writing to: " << options().bytecode_file
                         << std::endl;
      }

      return 0;
    });

    return p;
  }
}
