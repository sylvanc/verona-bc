#include "../../include/vrt/object.h"
#include "codegen.h"

#include <algorithm>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalVariable.h>
#include <utility>

namespace vbcc
{
  namespace llvm_backend
  {
    bool LLVMCodegen::declare_class_types()
    {
      for (std::size_t index = 0; index < state.classes.size(); ++index)
      {
        const auto& definition = state.classes.at(index);
        auto class_id = definition / ClassId;
        auto name = node_text(class_id);
        auto* payload_type = llvm::StructType::create(
          context, "verona.class." + std::to_string(index) + ".payload");

        if (!classes
               .emplace(
                 name,
                 LoweredClass{
                   NumPrimitiveClasses + index, payload_type, {}, nullptr})
               .second)
        {
          fail(class_id, "duplicate LLVM class '" + name + "'");
          return false;
        }
      }

      return true;
    }

    bool LLVMCodegen::define_class_types()
    {
      for (const auto& definition : state.classes)
      {
        auto class_id = definition / ClassId;
        auto lowered = classes.find(node_text(class_id));

        if (lowered == classes.end())
        {
          fail(class_id, "LLVM class was not declared");
          return false;
        }

        auto& lowered_class = lowered->second;
        std::vector<llvm::Type*> payload_fields;
        auto fields = definition / Fields;
        payload_fields.reserve(fields->size());
        lowered_class.field_types.reserve(fields->size());

        for (const auto& field : *fields)
        {
          auto field_type = lower_type(field / Type);
          if (!field_type)
            return false;

          if (field_type->runtime_type == vrt::ValueType::none)
          {
            payload_fields.push_back(
              llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 0));
            lowered_class.field_types.push_back(*field_type);
            continue;
          }

          if (field_type->storage_type == nullptr)
          {
            fail(field, "class field has no LLVM storage representation");
            return false;
          }

          payload_fields.push_back(field_type->storage_type);
          lowered_class.field_types.push_back(*field_type);
        }

        lowered_class.payload_type->setBody(payload_fields, false);
      }

      return true;
    }

    bool LLVMCodegen::define_class_metadata()
    {
      auto* word_type = module.getDataLayout().getIntPtrType(context);
      auto* pointer_type = llvm::PointerType::getUnqual(context);
      auto* field_metadata_type = llvm::StructType::get(
        context, {word_type, word_type, word_type, word_type});
      auto* method_metadata_type =
        llvm::StructType::get(context, {word_type, pointer_type});
      auto* class_metadata_type = llvm::StructType::get(
        context,
        {word_type,
         pointer_type,
         word_type,
         word_type,
         word_type,
         pointer_type,
         word_type,
         pointer_type,
         pointer_type});
      auto* null_pointer = llvm::ConstantPointerNull::get(pointer_type);

      const auto word = [word_type](std::size_t value) {
        return llvm::ConstantInt::get(word_type, value);
      };

      for (std::size_t index = 0; index < state.classes.size(); ++index)
      {
        const auto& definition = state.classes.at(index);
        auto class_id = definition / ClassId;
        auto class_name = node_text(class_id);
        auto lowered = classes.find(class_name);

        if (lowered == classes.end())
        {
          fail(class_id, "LLVM class layout is unavailable");
          return false;
        }

        auto& lowered_class = lowered->second;
        auto fields = definition / Fields;
        auto methods = definition / Methods;
        auto* payload_layout =
          module.getDataLayout().getStructLayout(lowered_class.payload_type);
        std::vector<llvm::Constant*> field_metadata;
        field_metadata.reserve(fields->size());

        for (std::size_t field_index = 0; field_index < fields->size();
             ++field_index)
        {
          const auto& field = fields->at(field_index);
          const auto& field_type = lowered_class.field_types.at(field_index);
          std::size_t type_id = 0;

          if (
            (field / Type)
              ->type()
              .in(
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
                 F32,
                 F64,
                 ILong,
                 ULong,
                 ISize,
                 USize,
                 Ptr}))
          {
            type_id = +val(field / Type);
          }
          else if ((field / Type) == ClassId)
          {
            auto field_class = classes.find(node_text(field / Type));
            if (field_class == classes.end())
            {
              fail(field / Type, "unknown class field type");
              return false;
            }

            type_id = field_class->second.type_id;
          }
          else
          {
            fail(field / Type, "class field type has no runtime type id");
            return false;
          }

          switch (field_type.runtime_type)
          {
            case vrt::ValueType::none:
            case vrt::ValueType::scalar:
            case vrt::ValueType::raw_pointer:
            case vrt::ValueType::object:
              break;

            default:
              fail(field / Type, "unsupported class field runtime type");
              return false;
          }

          auto field_size = field_type.runtime_type == vrt::ValueType::none ?
            0 :
            module.getDataLayout()
              .getTypeAllocSize(field_type.storage_type)
              .getFixedValue();
          field_metadata.push_back(
            llvm::ConstantStruct::get(
              field_metadata_type,
              {word(
                 payload_layout->getElementOffset(field_index).getFixedValue()),
               word(field_size),
               word(type_id),
               word(static_cast<std::size_t>(field_type.runtime_type))}));
        }

        llvm::Constant* fields_pointer = null_pointer;
        if (!field_metadata.empty())
        {
          auto* array_type =
            llvm::ArrayType::get(field_metadata_type, field_metadata.size());
          auto* array = new llvm::GlobalVariable(
            module,
            array_type,
            true,
            llvm::GlobalValue::PrivateLinkage,
            llvm::ConstantArray::get(array_type, field_metadata),
            "verona.class." + std::to_string(index) + ".fields");
          fields_pointer = array;
        }

        std::vector<std::pair<std::size_t, llvm::Constant*>> sorted_methods;
        sorted_methods.reserve(methods->size());
        for (const auto& method : *methods)
        {
          auto function = functions.find(node_text(method / FunctionId));
          if (function == functions.end())
          {
            fail(method / FunctionId, "class method function is unavailable");
            return false;
          }

          if (function->second.descriptor == nullptr)
          {
            fail(
              method / FunctionId,
              "class method callable metadata is unavailable");
            return false;
          }

          auto method_name = ST::di().string(method / MethodId);
          auto method_id = state.method_ids.find(method_name);
          if (method_id == state.method_ids.end())
          {
            fail(method / MethodId, "class method id is unavailable");
            return false;
          }

          sorted_methods.emplace_back(
            method_id->second,
            llvm::ConstantStruct::get(
              method_metadata_type,
              {word(method_id->second), function->second.descriptor}));
        }

        std::sort(
          sorted_methods.begin(),
          sorted_methods.end(),
          [](const auto& lhs, const auto& rhs) {
            return lhs.first < rhs.first;
          });

        std::vector<llvm::Constant*> method_metadata;
        method_metadata.reserve(sorted_methods.size());
        for (std::size_t method_index = 0; method_index < sorted_methods.size();
             ++method_index)
        {
          if (
            (method_index != 0) &&
            (sorted_methods[method_index - 1].first ==
             sorted_methods[method_index].first))
          {
            fail(methods, "duplicate method id in LLVM class metadata");
            return false;
          }

          method_metadata.push_back(sorted_methods[method_index].second);
        }

        llvm::Constant* methods_pointer = null_pointer;
        if (!method_metadata.empty())
        {
          auto* array_type =
            llvm::ArrayType::get(method_metadata_type, method_metadata.size());
          auto* array = new llvm::GlobalVariable(
            module,
            array_type,
            true,
            llvm::GlobalValue::PrivateLinkage,
            llvm::ConstantArray::get(array_type, method_metadata),
            "verona.class." + std::to_string(index) + ".methods");
          methods_pointer = array;
        }

        auto* name_value =
          llvm::ConstantDataArray::getString(context, class_name);
        auto* name_global = new llvm::GlobalVariable(
          module,
          name_value->getType(),
          true,
          llvm::GlobalValue::PrivateLinkage,
          name_value,
          "verona.class." + std::to_string(index) + ".name");
        name_global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);

        auto* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
        llvm::Constant* name_indices[] = {zero, zero};
        auto* name_pointer = llvm::ConstantExpr::getInBoundsGetElementPtr(
          name_value->getType(), name_global, name_indices);
        auto* metadata = llvm::ConstantStruct::get(
          class_metadata_type,
          {word(lowered_class.type_id),
           name_pointer,
           word(payload_layout->getSizeInBytes().getFixedValue()),
           word(module.getDataLayout()
                  .getABITypeAlign(lowered_class.payload_type)
                  .value()),
           word(fields->size()),
           fields_pointer,
           word(methods->size()),
           methods_pointer,
           null_pointer});
        lowered_class.descriptor = new llvm::GlobalVariable(
          module,
          class_metadata_type,
          true,
          llvm::GlobalValue::PrivateLinkage,
          metadata,
          "verona.class." + std::to_string(index) + ".descriptor");
      }

      return true;
    }
  }
}
