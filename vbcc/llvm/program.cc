#include "codegen.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <map>

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::declare_program_entry()
    {
      auto main = functions.find("@main");

      if (main == functions.end())
        return true;

      if (module.getFunction("verona_program_entry"))
      {
        fail(state.top, "duplicate LLVM function name 'verona_program_entry'");
        return false;
      }

      auto* entry_type =
        llvm::FunctionType::get(llvm::Type::getVoidTy(context), {}, false);
      program_entry = llvm::Function::Create(
        entry_type,
        llvm::GlobalValue::ExternalLinkage,
        "verona_program_entry",
        module);
      program_entry->setCallingConv(llvm::CallingConv::C);
      return true;
    }

    bool LLVMCodegen::define_program_metadata()
    {
      if (module.getNamedGlobal("verona_program") != nullptr)
      {
        fail(state.top, "duplicate LLVM program descriptor");
        return false;
      }

      auto* word_type = module.getDataLayout().getIntPtrType(context);
      auto* pointer_type = llvm::PointerType::getUnqual(context);
      auto* type_metadata_type =
        llvm::StructType::get(
          context, {word_type, word_type, word_type, word_type});
      auto* singleton_metadata_type =
        llvm::StructType::get(context, {pointer_type, pointer_type});
      auto* program_metadata_type = llvm::StructType::get(
        context, {word_type, pointer_type, word_type, pointer_type});

      struct EmittedType
      {
        vrt::ValueType value_type;
        std::size_t storage_size;
        std::size_t element_type_id;
      };

      std::map<std::size_t, EmittedType> types;
      const auto metadata_for = [&](const Node& type)
        -> std::optional<EmittedType> {
        auto lowered = lower_type(type);
        if (!lowered)
          return {};

        std::size_t storage_size = 0;
        if (lowered->runtime_type != vrt::ValueType::none)
        {
          if (lowered->storage_type == nullptr)
          {
            fail(type, "runtime type has no storage representation");
            return {};
          }

          storage_size = module.getDataLayout()
                           .getTypeAllocSize(lowered->storage_type)
                           .getFixedValue();
        }

        std::size_t element_type_id = 0;
        if (type == Array)
        {
          if (type->size() != 1)
          {
            fail(type, "array runtime type requires one element type");
            return {};
          }

          auto element_id = runtime_type_id(type / Type);
          if (!element_id)
            return {};

          element_type_id = *element_id;
        }

        return EmittedType{
          lowered->runtime_type, storage_size, element_type_id};
      };

      const auto add_primitive = [&](const auto& token) {
        Node type = token;
        auto metadata = metadata_for(type);
        if (!metadata)
          return false;

        return types.emplace(+val(type), *metadata).second;
      };

      if (
        !add_primitive(None) || !add_primitive(Bool) || !add_primitive(I8) ||
        !add_primitive(I16) || !add_primitive(I32) || !add_primitive(I64) ||
        !add_primitive(U8) || !add_primitive(U16) || !add_primitive(U32) ||
        !add_primitive(U64) || !add_primitive(ILong) ||
        !add_primitive(ULong) || !add_primitive(ISize) ||
        !add_primitive(USize) || !add_primitive(F32) ||
        !add_primitive(F64) || !add_primitive(Ptr))
      {
        fail(state.top, "duplicate primitive runtime type metadata ID");
        return false;
      }

      for (const auto& [name, cls] : classes)
      {
        (void)name;
        if (
          !types
             .emplace(
               cls.type_id,
               EmittedType{
                 vrt::ValueType::object,
                 module.getDataLayout().getPointerSize(),
                 0})
             .second)
        {
          fail(state.top, "duplicate nominal runtime type metadata ID");
          return false;
        }
      }

      const auto complex_base = state.classes.size() + NumPrimitiveClasses;
      for (std::size_t index = 0; index < state.complex_primitives.size();
           ++index)
      {
        const auto& primitive = state.complex_primitives.at(index);
        if (!primitive || ((primitive / Type) != Array))
          continue;

        auto metadata = metadata_for(primitive / Type);
        if (!metadata)
          return false;

        if (!types.emplace(complex_base + index, *metadata).second)
        {
          fail(state.top, "duplicate array runtime type metadata ID");
          return false;
        }
      }

      const auto runtime_base = complex_base + state.complex_primitives.size();
      for (std::size_t index = 0; index < runtime_types.size(); ++index)
      {
        auto metadata = metadata_for(runtime_types.at(index));
        if (!metadata)
          return false;

        if (!types.emplace(runtime_base + index, *metadata).second)
        {
          fail(state.top, "duplicate runtime type metadata ID");
          return false;
        }
      }

      const auto word = [word_type](std::size_t value) {
        return llvm::ConstantInt::get(word_type, value);
      };
      std::vector<llvm::Constant*> type_metadata;
      type_metadata.reserve(types.size());
      for (const auto& [type_id, metadata] : types)
      {
        type_metadata.push_back(
          llvm::ConstantStruct::get(
            type_metadata_type,
            {word(type_id),
             word(static_cast<std::size_t>(metadata.value_type)),
             word(metadata.storage_size),
             word(metadata.element_type_id)}));
      }

      auto* null_pointer = llvm::ConstantPointerNull::get(pointer_type);
      llvm::Constant* types_pointer = null_pointer;
      if (!type_metadata.empty())
      {
        auto* array_type =
          llvm::ArrayType::get(type_metadata_type, type_metadata.size());
        auto* type_table = new llvm::GlobalVariable(
          module,
          array_type,
          true,
          llvm::GlobalValue::PrivateLinkage,
          llvm::ConstantArray::get(array_type, type_metadata),
          "verona.program.types");
        types_pointer = type_table;
      }

      llvm::Constant* singletons_pointer = null_pointer;
      if (!singletons.empty())
      {
        std::vector<llvm::Constant*> singleton_metadata;
        singleton_metadata.reserve(singletons.size());
        for (const auto& singleton : singletons)
        {
          singleton_metadata.push_back(
            llvm::ConstantStruct::get(
              singleton_metadata_type, {singleton.storage, singleton.cls}));
        }

        auto* array_type =
          llvm::ArrayType::get(singleton_metadata_type, singletons.size());
        singletons_pointer = new llvm::GlobalVariable(
          module,
          array_type,
          true,
          llvm::GlobalValue::PrivateLinkage,
          llvm::ConstantArray::get(array_type, singleton_metadata),
          "verona.program.singletons");
      }

      auto* program_metadata = llvm::ConstantStruct::get(
        program_metadata_type,
        {word(type_metadata.size()),
         types_pointer,
         word(singletons.size()),
         singletons_pointer});

      new llvm::GlobalVariable(
        module,
        program_metadata_type,
        true,
        llvm::GlobalValue::ExternalLinkage,
        program_metadata,
        "verona_program");
      return true;
    }

    bool LLVMCodegen::emit_program_entry()
    {
      if (program_entry == nullptr)
        return true;

      auto main = functions.find("@main");

      if (main == functions.end())
      {
        fail(state.top, "LLVM main function is unavailable");
        return false;
      }

      if (
        (main->second.function == nullptr) ||
        (main->second.descriptor == nullptr))
      {
        fail(state.top, "LLVM entry function is unavailable");
        return false;
      }

      auto* entry = llvm::BasicBlock::Create(context, "entry", program_entry);

      builder.SetInsertPoint(entry);
      if (!emit_enter_frame(state.top, main->second.descriptor))
        return false;

      auto* call = builder.CreateCall(main->second.function);
      call->setCallingConv(main->second.function->getCallingConv());
      builder.CreateRetVoid();
      return true;
    }
  }
}
