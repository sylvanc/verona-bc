#include "codegen.h"

#include <llvm/Support/raw_ostream.h>

namespace vbcc
{
  namespace llvm_backend
  {
    std::optional<std::size_t>
    LLVMCodegen::runtime_type_id(const Node& input)
    {
      auto type = input;

      while (type == TypeId)
      {
        bool resolved = false;

        for (const auto& alias : state.typealiases)
        {
          if ((alias / TypeId)->location() == type->location())
          {
            type = alias / Type;
            resolved = true;
            break;
          }
        }

        if (!resolved)
        {
          fail(type, "runtime metadata uses an unknown type alias");
          return {};
        }
      }

      if (type->type().in(
            {None,
             Bool,
             I8,
             I16,
             I32,
             I64,
             U8,
             U16,
             U32,
             U64,
             ILong,
             ULong,
             ISize,
             USize,
             F32,
             F64,
             Ptr}))
        return +val(type);

      if (type == ClassId)
      {
        auto cls = classes.find(node_text(type));

        if (cls != classes.end())
          return cls->second.type_id;
      }
      else
      {
        const auto complex_base =
          state.classes.size() + NumPrimitiveClasses;

        for (std::size_t index = 0; index < state.complex_primitives.size();
             ++index)
        {
          const auto& primitive = state.complex_primitives.at(index);

          if (primitive && (primitive / Type)->equals(type))
            return complex_base + index;
        }

        for (std::size_t index = 0; index < runtime_types.size(); ++index)
        {
          if (runtime_types.at(index)->equals(type))
            return complex_base + state.complex_primitives.size() + index;
        }

        runtime_types.push_back(type);
        return complex_base + state.complex_primitives.size() +
          runtime_types.size() - 1;
      }

      fail(type, "type has no runtime metadata ID");
      return {};
    }

    std::string LLVMCodegen::node_text(const Node& node)
    {
      return std::string(node->location().view());
    }

    std::string LLVMCodegen::strip_sigil(const std::string& name)
    {
      if (
        !name.empty() &&
        ((name.front() == '@') || (name.front() == '$') ||
         (name.front() == '^')))
        return name.substr(1);

      return name;
    }

    void LLVMCodegen::fail(const Node& node, const std::string& message)
    {
      failed = true;
      llvm::errs() << "LLVM backend: " << message << " at "
                   << std::string(node->type().str()) << "\n";
    }
  }
}
