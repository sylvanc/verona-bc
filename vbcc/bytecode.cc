#include "lang.h"

#include "vbci.h"
#include "wf.h"

namespace vbcc
{
  PassDef bytecode()
  {
    using namespace vbci;

    struct State
    {
      bool error = false;
      FuncId func_id = 0;
      ClassId class_id = 0;
      FieldId field_id = 0;
      MethodId method_id = 0;

      std::unordered_map<std::string, FuncId> func_ids;
      std::unordered_map<std::string, ClassId> class_ids;
      std::unordered_map<std::string, FieldId> field_ids;
      std::unordered_map<std::string, MethodId> method_ids;

      std::vector<Node> functions;
      std::vector<Node> primitives;
      std::vector<Node> classes;

      std::optional<FuncId> get_func_id(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = func_ids.find(name);

        if (find == func_ids.end())
          return {};

        return find->second;
      }

      void add_func(Node func)
      {
        auto name = std::string((func / GlobalId)->location().view());
        func_ids.insert({name, func_id++});
        functions.push_back(func);
        assert(func_id == functions.size());
      }

      std::optional<ClassId> get_class_id(Node id)
      {
        auto name = std::string(id->location().view());
        auto find = class_ids.find(name);

        if (find == class_ids.end())
          return {};

        return find->second;
      }

      void add_class(Node cls)
      {
        auto name = std::string((cls / GlobalId)->location().view());
        class_ids.insert({name, class_id++});
        classes.push_back(cls);
        assert(class_id == classes.size());
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
          field_ids.insert({name, field_id++});
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
          method_ids.insert({name, method_id++});
      }
    };

    auto state = std::make_shared<State>();
    state->primitives.resize(NumPrimitiveClasses);

    PassDef p{
      "bytecode",
      wfPassLabels,
      dir::topdown | dir::once,
      {
        // Accumulate functions.
        T(Func)[Func] >> [state](Match& _) -> Node {
          auto func = _(Func);

          if (state->get_func_id(func / GlobalId))
          {
            state->error = true;
            return err(func / GlobalId, "duplicate function name");
          }

          state->add_func(func);
          return NoChange;
        },

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
          auto cls = _(Class);

          if (state->get_class_id(cls / GlobalId))
          {
            state->error = true;
            return err(cls / GlobalId, "duplicate class name");
          }

          state->add_class(cls);
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
      }};

    p.post([state](auto) {
      if (state->error)
        return 0;

      std::vector<Code> code;
      code.push_back(MagicNumber);
      code.push_back(CurrentVersion);

      // Functions.
      code.push_back(state->functions.size());

      for (auto& f : state->functions)
      {
        // TODO: 8 bit label count, 8 bit param count.
        // TODO: 64 bit PC for each label.
        (void)f;
      }

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
