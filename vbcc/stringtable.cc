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

  ST::Index ST::file(std::string_view str)
  {
    return string(options().relative(str).string());
  }

  ST::Index ST::file(Node node)
  {
    return file(node->location().view());
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
