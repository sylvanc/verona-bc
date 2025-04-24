#include "stringtable.h"

#include "lang.h"

namespace vbcc
{
  ST::Index ST::file(std::string_view str)
  {
    return pub_instance().intern(options().relative(str).string());
  }

  ST::Index ST::file(Node node)
  {
    return pub_instance().intern(
      options().relative(node->location().view()).string());
  }

  ST::Index ST::pub(std::string_view str)
  {
    return pub_instance().intern(str);
  }

  ST::Index ST::pub(Node node)
  {
    return pub_instance().intern(node->location().view());
  }

  ST::Index ST::priv(std::string_view str)
  {
    return priv_instance().intern(str);
  }

  ST::Index ST::priv(Node node)
  {
    return priv_instance().intern(node->location().view());
  }

  size_t ST::size()
  {
    return pub_instance().store.size();
  }

  const std::string& ST::at(size_t i)
  {
    return pub_instance().store.at(i);
  }

  ST& ST::pub_instance()
  {
    static ST st;
    return st;
  }

  ST& ST::priv_instance()
  {
    static ST st;
    return st;
  }

  ST::Index ST::intern(std::string_view str)
  {
    auto find = lookup.find(str);

    if (find != lookup.end())
      return find->second;

    auto id = store.size();
    store.emplace_back(std::string(str));
    lookup.emplace(store.back(), id);
    return id;
  }
}
