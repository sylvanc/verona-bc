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

    static Index file(std::string_view str);
    static Index file(Node node);
    static Index pub(std::string_view str);
    static Index pub(Node node);
    static Index priv(std::string_view str);
    static Index priv(Node node);
    static size_t size();
    static const std::string& at(size_t i);

  private:
    std::deque<std::string> store;
    std::unordered_map<std::string_view, Index> lookup;

    static ST& pub_instance();
    static ST& priv_instance();

    Index intern(std::string_view str);
  };
}
