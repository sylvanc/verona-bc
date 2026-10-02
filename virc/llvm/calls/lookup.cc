#include "../codegen.h"

#include <functional>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Instructions.h>
#include <set>

namespace virc
{
  namespace llvm_backend
  {
    std::optional<LookupPlan> LLVMCodegen::resolve_lookup(const Node& statement)
    {
      auto method_id = statement / MethodId;
      std::set<std::size_t> target_class_ids;
      std::set<std::string> resolving_aliases;
      std::vector<LookupTarget> targets;
      std::optional<LoweredSignature> signature;
      bool collection_failed = false;

      const auto collect_class = [&](const Node& definition) -> bool {
        auto class_name = node_text(definition / ClassId);
        auto lowered_class = classes.find(class_name);

        if (lowered_class == classes.end())
        {
          fail(
            definition / ClassId,
            "dynamic lookup class '" + class_name + "' is unavailable");
          collection_failed = true;
          return false;
        }

        auto class_id = lowered_class->second.type_id;
        if (!target_class_ids.insert(class_id).second)
          return true;

        for (const auto& method : *(definition / Methods))
        {
          if ((method / MethodId)->location() == method_id->location())
          {
            auto function_id = node_text(method / FunctionId);
            auto function = functions.find(function_id);

            if (function == functions.end())
            {
              fail(
                method / FunctionId,
                "dynamic method function '" + function_id + "' is unavailable");
              collection_failed = true;
              return false;
            }

            if (function->second.descriptor == nullptr)
            {
              fail(
                method / FunctionId,
                "dynamic method function '" + function_id +
                  "' has no callable descriptor");
              collection_failed = true;
              return false;
            }

            const auto& target_signature = function->second.signature;

            if (signature && (*signature != target_signature))
            {
              fail(
                statement,
                "dynamic method implementations have incompatible LLVM "
                "signatures");
              collection_failed = true;
              return false;
            }

            signature = target_signature;
            targets.push_back({class_id, function->second.descriptor});
            return true;
          }
        }

        fail(
          definition,
          "dynamic method '" + node_text(method_id) +
            "' has no implementation on class '" + class_name + "'");
        collection_failed = true;
        return false;
      };

      std::function<bool(const Node&)> collect_type;
      collect_type = [&](const Node& type) {
        if (!type)
          return false;

        if (type == ClassId)
        {
          for (const auto& definition : state.classes)
          {
            if ((definition / ClassId)->location() == type->location())
              return collect_class(definition);
          }

          return false;
        }

        if (type == TypeId)
        {
          auto alias_name = node_text(type);
          if (!resolving_aliases.insert(alias_name).second)
          {
            fail(type, "cyclic type alias '" + alias_name + "'");
            collection_failed = true;
            return false;
          }

          for (const auto& alias : state.typealiases)
          {
            if ((alias / TypeId)->location() == type->location())
            {
              auto resolved = collect_type(alias / Type);
              resolving_aliases.erase(alias_name);
              return resolved;
            }
          }

          resolving_aliases.erase(alias_name);
          return false;
        }

        if (type == Union)
        {
          bool resolved = true;

          for (const auto& member : *type)
            resolved = collect_type(member) && resolved;

          return resolved;
        }

        return false;
      };

      bool resolved = false;
      auto function = statement->parent(Func);

      if (!function)
        function = statement->parent(FuncOnce);

      if (function)
      {
        auto lookups =
          state.func_lookups.find(node_text(function / FunctionId));

        if (lookups != state.func_lookups.end())
        {
          auto lookup = lookups->second.find(node_text(statement / LocalId));

          if (lookup != lookups->second.end())
            resolved = collect_type(lookup->second.src_type);
        }
      }

      if (!resolved)
      {
        if (!collection_failed)
          fail(statement, "dynamic lookup type metadata is unavailable");
        return {};
      }

      if (!signature)
      {
        fail(
          statement,
          "dynamic method '" + node_text(method_id) +
            "' has no callable implementation");
        return {};
      }

      return LookupPlan{std::move(*signature), std::move(targets)};
    }

    bool LLVMCodegen::emit_lookup(const Node& statement)
    {
      constexpr std::size_t max_switch_candidates = 8;
      auto dst = statement / LocalId;
      auto receiver = locals.find_value(statement / Rhs);

      if (!receiver)
      {
        fail(
          statement,
          "lookup from unknown local '" + node_text(statement / Rhs) + "'");
        return false;
      }

      if (
        (receiver->type.runtime_type != vrt::ValueType::object) ||
        (receiver->value == nullptr))
      {
        fail(statement, "dynamic lookup receiver is not an object");
        return false;
      }

      if (runtime.object_lookup == nullptr)
      {
        fail(statement, "LLVM dynamic lookup runtime is unavailable");
        return false;
      }

      auto method_name = ST::di().string(statement / MethodId);
      auto method = state.method_ids.find(method_name);

      if (method == state.method_ids.end())
      {
        fail(statement / MethodId, "dynamic lookup method id is unavailable");
        return false;
      }

      auto plan = resolve_lookup(statement);

      if (!plan)
        return false;

      auto* pointer_type = llvm::PointerType::getUnqual(context);
      auto* word_type = module.getDataLayout().getIntPtrType(context);
      auto* method_value = llvm::ConstantInt::get(word_type, method->second);
      llvm::Value* callable = nullptr;

      if (plan->targets.size() == 1)
      {
        callable = plan->targets.front().descriptor;
      }
      else if (plan->targets.size() <= max_switch_candidates)
      {
        if (runtime.object_class_id == nullptr)
        {
          fail(statement, "LLVM object class ID runtime is unavailable");
          return false;
        }

        auto name = strip_sigil(node_text(dst));
        auto* function = builder.GetInsertBlock()->getParent();
        auto* fallback = llvm::BasicBlock::Create(
          context, name + ".lookup.fallback", function);
        auto* merge =
          llvm::BasicBlock::Create(context, name + ".lookup.merge", function);
        auto* class_id = builder.CreateCall(
          runtime.object_class_id, {receiver->value}, name + ".class-id");
        auto* dispatch =
          builder.CreateSwitch(class_id, fallback, plan->targets.size());

        std::vector<std::pair<llvm::BasicBlock*, llvm::Value*>> incoming;
        incoming.reserve(plan->targets.size() + 1);

        for (std::size_t index = 0; index < plan->targets.size(); ++index)
        {
          const auto& target = plan->targets[index];
          auto* case_block =
            llvm::BasicBlock::Create(context, name + ".lookup.case", function);
          dispatch->addCase(
            llvm::ConstantInt::get(word_type, target.class_id), case_block);
          builder.SetInsertPoint(case_block);
          builder.CreateBr(merge);
          incoming.emplace_back(case_block, target.descriptor);
        }

        builder.SetInsertPoint(fallback);
        auto* fallback_callable = builder.CreateCall(
          runtime.object_lookup,
          {receiver->value, method_value},
          name + ".fallback");
        builder.CreateBr(merge);
        incoming.emplace_back(fallback, fallback_callable);

        builder.SetInsertPoint(merge);
        auto* callable_phi =
          builder.CreatePHI(pointer_type, incoming.size(), name);
        for (const auto& [block, value] : incoming)
          callable_phi->addIncoming(value, block);
        callable = callable_phi;
      }
      else
      {
        callable = builder.CreateCall(
          runtime.object_lookup,
          {receiver->value, method_value},
          strip_sigil(node_text(dst)));
      }

      LoweredType callable_type{
        IRValueType::Function,
        vrt::ValueType::raw_pointer,
        pointer_type,
        pointer_type};

      return locals.bind_value(
        statement,
        dst,
        LoweredValue{
          callable_type,
          callable,
          std::optional<LoweredSignature>{std::move(plan->signature)}});
    }
  }
}
