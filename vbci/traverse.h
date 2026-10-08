#pragma once

#include "array.h"
#include "header.h"
#include "object.h"
#include "program.h"

#include <vector>

namespace vbci
{
  namespace detail
  {
    struct NopPost
    {
      void operator()(Header*) {}
    };
  }

  template<typename Pre, typename Post = detail::NopPost>
  void traverse(Header* root, Pre pre, Post post = detail::NopPost())
  {
    struct Work
    {
      Header* header;
      bool post;
    };

    if (!pre(root))
      return;

    std::vector<Work> worklist{{root, false}};
    auto& program = Program::get();

    while (!worklist.empty())
    {
      auto [header, is_post] = worklist.back();
      worklist.pop_back();

      if (is_post)
      {
        post(header);
        continue;
      }

      worklist.push_back({header, true});

      auto add_child = [&](Header* child) {
        if (pre(child))
          worklist.push_back({child, false});
      };
      if (program.is_array(header->get_type_id()))
        static_cast<Array*>(header)->trace_fn(add_child);
      else
        static_cast<Object*>(header)->trace_fn(add_child);
    }
  }
}
