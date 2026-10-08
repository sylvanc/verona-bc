#include "copy_graph.h"

#include "array.h"
#include "frame.h"
#include "object.h"
#include "program.h"
#include "thread.h"
#include "traverse.h"

#include <unordered_map>
#include <vector>

namespace vbci
{
  namespace
  {
    struct GraphCopy
    {
      Frame& frame;
      std::unordered_map<Header*, Header*> copies;
      std::vector<std::pair<Function*, Object*>> post_copies;

      GraphCopy(Frame& frame) : frame(frame) {}

      Header* allocate(Header* source)
      {
        auto& program = Program::get();
        Header* copy;

        if (program.is_array(source->get_type_id()))
        {
          auto source_array = static_cast<Array*>(source);
          copy =
            frame.region->array(source->get_type_id(), source_array->get_size());
        }
        else
        {
          auto source_object = static_cast<Object*>(source);
          copy = frame.region->object(source_object->cls());
        }

        copies.emplace(source, copy);
        return copy;
      }

      Register map_value(Value value)
      {
        // Cowns still need a new reference; primitives make this a no-op.
        if (!value.is_header())
          return ValueBorrow(value);

        auto source = value.get_header();
        auto loc = source->location();
        if (loc.is_immutable() || loc.is_immortal())
          return ValueTransfer(value);

        auto target = copies.at(source);
        if (value.is_array())
          return ValueTransfer(Value(static_cast<Array*>(target)));

        return ValueTransfer(Value(static_cast<Object*>(target)));
      }

      void fill(Header* source)
      {
        auto& program = Program::get();
        auto copy = copies.at(source);

        if (program.is_array(source->get_type_id()))
        {
          auto source_array = static_cast<Array*>(source);
          auto copy_array = static_cast<Array*>(copy);

          for (size_t i = 0; i < source_array->get_size(); i++)
            copy_array->initialize(i, map_value(source_array->load(i)));
        }
        else
        {
          auto source_object = static_cast<Object*>(source);
          auto copy_object = static_cast<Object*>(copy);

          for (size_t i = 0; i < source_object->cls().fields.size(); i++)
            copy_object->initialize(i, map_value(source_object->load(i)));

          if (auto post_copy = copy_object->cls().post_copy())
            post_copies.emplace_back(post_copy, copy_object);
        }
      }

      Register run(ValueBorrow root)
      {
        auto root_header = root.get_header();
        traverse(
          root_header,
          [&](Header* source) {
            auto loc = source->location();
            if (loc.is_immutable() || loc.is_immortal())
            {
              source->field_inc();
              return false;
            }

            if (auto copy = copies.find(source); copy != copies.end())
            {
              copy->second->field_inc();
              return false;
            }

            allocate(source);
            return true;
          },
          [&](Header* source) { fill(source); });

        // A hook can follow a back edge to a copy that has not yet been filled.
        for (auto [post_copy, copy] : post_copies)
          Thread::run_sync(post_copy, ValueBorrow(copy));

        return map_value(root);
      }
    };
  }

  Register copy_graph(Frame& frame, ValueBorrow root)
  {
    return GraphCopy(frame).run(root);
  }
}
