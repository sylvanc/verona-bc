#include "emitter.h"
#include "encoder.h"
#include "instruction_encoder.h"
#include "string_table.h"
#include "type_encoding.h"

#include "../lang.h"

#include <vbc/format.h>
#include <zstd.h>

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
    std::vector<uint8_t> di;
    std::vector<uint8_t> code;
    std::map<ST::Index, trieste::Source> di_source;

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
      di << uleb(ST::di().string(c / ClassId));

      auto fields = c / Fields;
      hdr << uleb(fields->size());

      for (auto& field : *fields)
      {
        hdr << uleb(*get_field_id(field / FieldId));
        hdr << uleb(type_id(field / Type));
        di << uleb(ST::di().string(field / FieldId));
      }

      auto methods = c / Methods;
      hdr << uleb(methods->size());

      for (auto& method : *methods)
      {
        hdr << uleb(*get_method_id(method / MethodId))
            << uleb(*get_func_id(method / FunctionId));
        di << uleb(ST::di().string(method / MethodId));
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
      di << uleb(func_state.name);

      // Register names.
      for (auto& name : func_state.register_names)
        di << uleb(name);


      constexpr size_t no_value = size_t(-1);
      size_t di_file = no_value;
      size_t di_offset = 0;
      size_t di_last_pc = code.size();
      bool explicit_di = false;

      // Keep track of all included source files.
      auto di_source_curr = di_source.end();

      auto adv_di = [&]() {
        auto di_cur_pc = code.size();

        if (di_cur_pc > di_last_pc)
        {
          di << d(DIOp::Skip, di_cur_pc - di_last_pc);
          di_last_pc = di_cur_pc;
        }
      };

      auto stmt_di = [&](Node& stmt) {
        // Record nothing for empty or synthetic source locations.
        if (
          !stmt->location().source || stmt->location().source->origin().empty())
          return;

        // Use the source and offset in the AST.
        if (
          (di_source_curr == di_source.end()) ||
          (di_source_curr->second != stmt->location().source))
        {
          // Pick a non-relative path.
          std::filesystem::path rel_path;

          for (auto& path : source_paths)
          {
            rel_path = std::filesystem::relative(
              stmt->location().source->origin(), path);

            if (!rel_path.empty() && (rel_path.c_str()[0] != '.'))
              break;
          }

          if (rel_path.empty() || (rel_path.c_str()[0] == '.'))
            rel_path = stmt->location().source->origin();

          di_source_curr =
            di_source
              .emplace(
                ST::di().string(rel_path.string()), stmt->location().source)
              .first;

          adv_di();
          di << d(DIOp::File, di_source_curr->first);
          di_file = di_source_curr->first;
          di_offset = 0;
        }

        auto pos = stmt->location().pos;

        if (pos != di_offset)
        {
          // Offset will also advance the PC by one, so reduce any Skip by one.
          di_last_pc++;
          adv_di();
          di << d(DIOp::Offset, pos - di_offset);
          di_offset = pos;
        }
      };

      for (auto label : *(func_state.func / Labels))
      {
        // Save the pc for this label.
        hdr << uleb(code.size());

        for (Node stmt : *(label / Body))
        {
          if (stmt == Source)
          {
            adv_di();
            di_file = ST::di().string(stmt / String);
            di_offset = 0;
            explicit_di = true;
            di << d(DIOp::File, di_file);
            continue;
          }
          else if (stmt == Offset)
          {
            adv_di();
            di_offset = from_chars_sep_v<size_t>(stmt / Int);
            explicit_di = true;
            di << d(DIOp::Offset, di_offset);
            continue;
          }
          else if (!explicit_di)
          {
            stmt_di(stmt);
          }

          encode_statement(*this, func_state, memo_slot_map, code, stmt);
        }

        Node term = label / Return;

        if (explicit_di)
          adv_di();
        else
          stmt_di(term);

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
    {
      std::vector<uint8_t> di_strs;

      // Debug info string table.
      di_strs << uleb(ST::di().size());

      for (size_t i = 0; i < ST::di().size(); i++)
        di_strs << ST::di().at(i);

      // Debug info source files.
      di_strs << uleb(di_source.size());

      for (auto& [id, source] : di_source)
      {
        di_strs << uleb(id);
        di_strs << source->view();
      }

      // Debug info ops.
      di_strs.insert(di_strs.end(), di.begin(), di.end());

      // Compress debug info.
      auto cap = ZSTD_compressBound(di_strs.size());
      di.resize(cap);
      auto compressed_size =
        ZSTD_compress(di.data(), cap, di_strs.data(), di_strs.size(), 12);

      if (!ZSTD_isError(compressed_size))
      {
        f.write(reinterpret_cast<const char*>(di.data()), compressed_size);
      }
      else
      {
        logging::Error() << "Error compressing debug info for: " << output
                         << std::endl;
      }
    }

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
