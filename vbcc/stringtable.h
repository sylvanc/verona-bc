#pragma once

#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vbcc.h>

namespace vbcc
{
  struct ST
  {
    using Index = size_t;

    static Index pub(std::string_view str)
    {
      return pub_instance().intern(str);
    }

    static Index pub(Node node)
    {
      return pub_instance().intern(node->location().view());
    }

    static Index priv(std::string_view str)
    {
      return priv_instance().intern(str);
    }

    static Index priv(Node node)
    {
      return priv_instance().intern(node->location().view());
    }

    static size_t size()
    {
      return pub_instance().store.size();
    }

    static const std::string& at(size_t i)
    {
      return pub_instance().store.at(i);
    }

  private:
    std::deque<std::string> store;
    std::unordered_map<std::string_view, Index> lookup;

    static ST& pub_instance()
    {
      static ST st;
      return st;
    }

    static ST& priv_instance()
    {
      static ST st;
      return st;
    }

    Index intern(std::string_view str)
    {
      auto find = lookup.find(str);

      if (find != lookup.end())
        return find->second;

      auto id = store.size();
      store.emplace_back(std::string(str));
      lookup.emplace(store.back(), id);
      return id;
    }
  };
}
