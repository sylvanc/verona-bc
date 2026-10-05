#pragma once

#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vir.h>

namespace virc
{
  using namespace vir;

  struct ST
  {
    using Index = size_t;

  private:
    std::deque<std::string> store;
    std::unordered_map<std::string_view, Index> lookup;

  public:
    static ST& noemit();
    static ST& exec();
    static ST& di();

    Index string(std::string_view str);
    Index string(Node node);

    size_t size();
    const std::string& at(size_t i);
  };
}
