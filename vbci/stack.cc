#include "stack.h"

#include "array.h"

namespace vbci
{
  Stack::Idx Stack::save()
  {
    return top;
  }

  void Stack::restore(const Idx& idx)
  {
    top = idx;
  }

  void* Stack::alloc(size_t size)
  {
    if (size > ChunkSize)
      return nullptr;

    if ((top.offset + size) > ChunkSize)
    {
      top.chunk++;
      top.offset = 0;
    }

    if (top.chunk >= chunks.size())
    {
      chunks.emplace_back();
      top.chunk = chunks.size() - 1;
      top.offset = 0;
    }

    auto ret = &chunks.at(top.chunk).at(top.offset);
    top.offset += size;
    return ret;
  }

  Array* Stack::array(Location frame_id, uint32_t type_id, size_t size)
  {
    auto& program = Program::get();
    auto content_type_id = program.unarray(type_id);
    auto rep = program.layout_type_id(content_type_id);
    auto mem = alloc(Array::size_of(size, rep.second->size));
    return Array::create(
      mem, frame_id, type_id, rep.first, size, rep.second->size);
  }
}
