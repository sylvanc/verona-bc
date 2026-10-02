#include "debug_info.h"

#include "string_table.h"

#include <trieste/trieste.h>
#include <zstd.h>

namespace virc::vbc_backend
{
  using namespace trieste;
  using namespace ::vbc;

  size_t DebugInfo::size() const
  {
    return data.size();
  }

  ByteBuffer& DebugInfo::output()
  {
    return data;
  }

  void DebugInfo::begin_function()
  {
    current_source = {};
    source_offset = 0;
    last_pc = code.size();
    explicit_location = false;
  }

  void DebugInfo::advance()
  {
    auto current_pc = code.size();

    if (current_pc > last_pc)
    {
      data << d(DIOp::Skip, current_pc - last_pc);
      last_pc = current_pc;
    }
  }

  void DebugInfo::record_explicit_file(Node file)
  {
    advance();
    source_offset = 0;
    explicit_location = true;
    data << d(DIOp::File, ST::di().string(file));
  }

  void DebugInfo::record_explicit_offset(size_t offset)
  {
    advance();
    source_offset = offset;
    explicit_location = true;
    data << d(DIOp::Offset, source_offset);
  }

  void DebugInfo::record_statement(Node statement)
  {
    if (explicit_location)
      return;

    if (
      !statement->location().source ||
      statement->location().source->origin().empty())
      return;

    if (current_source != statement->location().source)
    {
      std::filesystem::path relative_path;

      for (const auto& path : source_paths)
      {
        relative_path = std::filesystem::relative(
          statement->location().source->origin(), path);

        if (!relative_path.empty() && (relative_path.c_str()[0] != '.'))
          break;
      }

      if (relative_path.empty() || (relative_path.c_str()[0] == '.'))
        relative_path = statement->location().source->origin();

      current_source = statement->location().source;
      auto [source, inserted] = sources.emplace(
        ST::di().string(relative_path.string()), current_source);
      static_cast<void>(inserted);

      advance();
      data << d(DIOp::File, source->first);
      source_offset = 0;
    }

    auto position = statement->location().pos;

    if (position != source_offset)
    {
      last_pc++;
      advance();
      data << d(DIOp::Offset, position - source_offset);
      source_offset = position;
    }
  }

  void DebugInfo::finish_function(Node terminator)
  {
    if (explicit_location)
      advance();
    else
      record_statement(terminator);
  }

  void DebugInfo::write_to(
    std::ofstream& output, const std::filesystem::path& output_path)
  {
    ByteBuffer payload;
    encode_string_table(payload, ST::di());

    payload << uleb(sources.size());

    for (const auto& [id, source] : sources)
    {
      payload << uleb(id);
      payload << source->view();
    }

    payload.insert(payload.end(), data.begin(), data.end());

    auto capacity = ZSTD_compressBound(payload.size());
    ByteBuffer compressed(capacity);
    auto compressed_size = ZSTD_compress(
      compressed.data(), capacity, payload.data(), payload.size(), 12);

    if (!ZSTD_isError(compressed_size))
    {
      output.write(
        reinterpret_cast<const char*>(compressed.data()), compressed_size);
    }
    else
    {
      logging::Error() << "Error compressing debug info for: " << output_path
                       << std::endl;
    }
  }
}