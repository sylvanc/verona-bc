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

  private:
    std::deque<std::string> store;
    std::unordered_map<std::string_view, Index> lookup;

  public:
    static ST& noemit();
    static ST& ffi();
    static ST& di();

    Index string(std::string_view str);
    Index string(Node node);
    Index file(std::string_view str);
    Index file(Node node);

    size_t size();
    const std::string& at(size_t i);
  };
}
