#include "stringtable.h"

#include "lang.h"

namespace vbcc
{
  ST& ST::noemit()
  {
    static ST st;
    return st;
  }

  ST& ST::exec()
  {
    static ST st;
    return st;
  }

  ST& ST::di()
  {
    static ST st;
    return st;
  }

  ST::Index ST::string(std::string_view str)
  {
    auto find = lookup.find(str);

    if (find != lookup.end())
      return find->second;

    auto id = store.size();
    store.emplace_back(std::string(str));
    lookup.emplace(store.back(), id);
    return id;
  }

  ST::Index ST::string(Node node)
  {
    return string(node->location().view());
  }

  ST::Index ST::file(const std::filesystem::path& path, std::string_view str)
  {
    auto r = std::filesystem::relative(str, path).string();
    return string(r);
  }

  ST::Index ST::file(const std::filesystem::path& path, Node node)
  {
    return file(path, node->location().view());
  }

  size_t ST::size()
  {
    return store.size();
  }

  const std::string& ST::at(size_t i)
  {
    return store.at(i);
  }
}
