#include "emitter.h"
#include "debug_info.h"
#include "encoder.h"
#include "instruction_encoder.h"
#include "string_table.h"
#include "type_encoding.h"

#include "../lang.h"

#include <vbc/format.h>

namespace virc
{
  using namespace ::vbc;
  using namespace vbc_backend;
  using ::vbc::CurrentVersion;
  using ::vbc::DIOp;
  using ::vbc::MagicNumber;
  using ::vbc::NumPrimitiveClasses;
  using ::vbc::Op;
  using ::vbc::RegionType;
  using ::vbc::TypeTag;

  namespace
  {
    struct VBCEmitter : Compilation
    {
      explicit VBCEmitter(const Compilation& bytecode) : Compilation(bytecode) {}

      void emit(std::filesystem::path output, bool strip);
    };
  }

  void VBCEmitter::emit(std::filesystem::path output, bool strip)
  {
    wf::push_back(wfIR);

    if (output.empty())
      output = "out.vbc";

    std::vector<uint8_t> hdr;
    std::vector<uint8_t> code;
    DebugInfo di{code, source_paths};

    // Build memo slot mapping: init FunctionId string → 0-based index.
    std::unordered_map<std::string, size_t> memo_slot_map;
    Node memo_init_node;
    for (auto& child : *top)
    {
      if (child == MemoInit)
      {
        memo_init_node = child;
        size_t idx = 0;
        for (auto& fid : *child)
        {
          memo_slot_map[std::string(fid->location().view())] = idx++;
        }
        break;
      }
    }

    hdr << uleb(MagicNumber);
    hdr << uleb(CurrentVersion);

    encode_string_table(hdr, ST::exec());

    // Class and complex primitive count.
    hdr << uleb(classes.size());
    hdr << uleb(complex_primitives.size());

    // Primitive classes.
    for (auto& p : primitives)
    {
      if (p)
      {
        auto methods = p / Methods;
        hdr << uleb(methods->size());

        for (auto& method : *methods)
        {
          hdr << uleb(*get_method_id(method / MethodId))
              << uleb(*get_func_id(method / FunctionId));
        }
      }
      else
      {
        hdr << uleb(0);
      }
    }

    // Classes.
    for (auto& c : classes)
    {
      hdr << uleb(di.size());
      di.output() << uleb(ST::di().string(c / ClassId));

      auto fields = c / Fields;
      hdr << uleb(fields->size());

      for (auto& field : *fields)
      {
        hdr << uleb(*get_field_id(field / FieldId));
        hdr << uleb(type_id(field / Type));
        di.output() << uleb(ST::di().string(field / FieldId));
      }

      auto methods = c / Methods;
      hdr << uleb(methods->size());

      for (auto& method : *methods)
      {
        hdr << uleb(*get_method_id(method / MethodId))
            << uleb(*get_func_id(method / FunctionId));
        di.output() << uleb(ST::di().string(method / MethodId));
      }
    }

    // Complex primitive classes.
    for (auto& p : complex_primitives)
    {
      if (p)
      {
        auto methods = p / Methods;
        hdr << uleb(methods->size());

        for (auto& method : *methods)
        {
          hdr << uleb(*get_method_id(method / MethodId))
              << uleb(*get_func_id(method / FunctionId));
        }
      }
      else
      {
        hdr << uleb(0);
      }
    }

    // FFI libraries.
    hdr << uleb(libraries.size());

    for (auto& lib : libraries)
    {
      hdr << uleb(ST::exec().string(lib / String));

      // Encode init function ID. 0 means no function, otherwise
      // func_id + 1.
      auto init = lib / InitFunc;
      if (init->type() == FunctionId)
        hdr << uleb(*get_func_id(init) + 1);
      else
        hdr << uleb(0);
    }

    hdr << uleb(symbols.size());

    for (auto& symbol : symbols)
    {
      hdr << uleb(*get_library_id(symbol->parent(Lib)))
          << uleb(ST::exec().string(symbol / Lhs))
          << uleb(ST::exec().string(symbol / Rhs))
          << uleb(((symbol / Vararg) == Vararg) ? 1 : 0)
          << uleb((symbol / FFIParams)->size());

      for (auto& param : *(symbol / FFIParams))
        hdr << uleb(type_id(param));

      hdr << uleb(type_id(symbol / Return));
    }

    // Functions.
    hdr << uleb(functions.size());

    for (auto& func_state : functions)
    {
      hdr << uleb(func_state.register_idxs.size());
      hdr << uleb(di.size());

      // Parameter and return types.
      hdr << uleb(func_state.params);

      for (auto& param : *(func_state.func / Params))
        hdr << uleb(type_id(param / Type));

      hdr << uleb(type_id(func_state.func / Type));

      // Variable types.
      auto vars_node = func_state.func / Vars;
      hdr << uleb(vars_node->size());

      for (auto& var : *vars_node)
        hdr << uleb(type_id(var / Type));

      // Labels.
      hdr << uleb(func_state.label_idxs.size());

      // Function name.
      di.output() << uleb(func_state.name);

      // Register names.
      for (auto& name : func_state.register_names)
        di.output() << uleb(name);

      di.begin_function();

      for (auto label : *(func_state.func / Labels))
      {
        // Save the pc for this label.
        hdr << uleb(code.size());

        for (Node stmt : *(label / Body))
        {
          if (stmt == Source)
          {
            di.record_explicit_file(stmt / String);
            continue;
          }
          else if (stmt == Offset)
          {
            di.record_explicit_offset(
              from_chars_sep_v<size_t>(stmt / Int));
            continue;
          }

          di.record_statement(stmt);

          encode_statement(*this, func_state, memo_slot_map, code, stmt);
        }

        Node term = label / Return;
        di.finish_function(term);
        encode_terminator(*this, func_state, code, term);
      }
    }

    encode_type_table(hdr, types);

    // Memo init list.
    if (memo_init_node)
    {
      hdr << uleb(memo_init_node->size());
      for (auto& fid : *memo_init_node)
        hdr << uleb(*get_func_id(fid));
    }
    else
    {
      hdr << uleb(0);
    }

    // Code size.
    hdr << uleb(code.size());
    std::ofstream f(output, std::ios::binary | std::ios::out);
    f.write(reinterpret_cast<const char*>(hdr.data()), hdr.size());
    f.write(reinterpret_cast<const char*>(code.data()), code.size());

    if (!strip)
      di.write_to(f, output);

    if (!f)
      logging::Error() << "Error writing to: " << output << std::endl;

    wf::pop_front();
  }

  void vbc_backend::emit(
    const Compilation& bytecode,
    const std::filesystem::path& output,
    bool strip)
  {
    VBCEmitter(bytecode).emit(output, strip);
  }
}


void virc::vbc::emit(
  const virc::Compilation& compilation,
  const std::filesystem::path& output,
  bool strip)
{
  virc::vbc_backend::emit(compilation, output, strip);
}
