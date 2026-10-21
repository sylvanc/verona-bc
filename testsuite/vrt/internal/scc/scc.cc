// See README.md for SCC lifetime coverage, runtime boundaries, and non-goals.

#include "frame.h"
#include "header.h"
#include "object.h"
#include "region.h"
#include "thread.h"
#include "vrt.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <thread>
#include <vector>
#include <vrt/program.h>

namespace
{
  constexpr uintptr_t node_class_id = 0x101;
  constexpr uintptr_t value_class_id = 0x102;

  const vrt::Function finalizer_function{2, "Node.final", nullptr};

  struct NodeFields
  {
    void* next;
    void* outgoing;
  };

  struct ValueFields
  {
    uint64_t value;
  };

  std::atomic<uintptr_t> finalizer_calls{0};

  void finalizer_thunk(void* data_address)
  {
    vrt_frame_enter(&finalizer_function);
    auto* fields = static_cast<NodeFields*>(data_address);
    if (fields->next != nullptr)
    {
      auto* sibling =
        vrt::Header::from_data(vrt::ValueType::object, fields->next);
      if (sibling->magic != vrt::Header::magic_value)
        std::abort();
    }

    finalizer_calls.fetch_add(1, std::memory_order_relaxed);
    vrt_frame_leave();
  }

  const vrt::Field node_fields[] = {
    {offsetof(NodeFields, next),
     sizeof(NodeFields::next),
     node_class_id,
     vrt::ValueType::object},
    {offsetof(NodeFields, outgoing),
     sizeof(NodeFields::outgoing),
     value_class_id,
     vrt::ValueType::object}};

  const vrt::Field value_fields[] = {
    {offsetof(ValueFields, value),
     sizeof(ValueFields::value),
     0,
     vrt::ValueType::scalar}};

  const vrt::Class node_class{
    node_class_id,
    "Node",
    sizeof(NodeFields),
    alignof(NodeFields),
    2,
    node_fields,
    0,
    nullptr,
    nullptr,
    &finalizer_thunk};

  const vrt::Class value_class{
    value_class_id,
    "Value",
    sizeof(ValueFields),
    alignof(ValueFields),
    1,
    value_fields,
    0,
    nullptr,
    nullptr};

  const vrt::TypeInfo types[] = {
    {node_class_id, vrt::ValueType::object, sizeof(void*), 0},
    {value_class_id, vrt::ValueType::object, sizeof(void*), 0}};
  const vrt::Program program{2, types, 0, nullptr};

  vrt::Object* new_node(vrt::Region* region)
  {
    return region->object(&node_class);
  }

  vrt::Object* new_target(vrt::Region* region, uint64_t value)
  {
    auto* result = region->object(&value_class);
    static_cast<ValueFields*>(result->fields())->value = value;
    return result;
  }

  void set_next(vrt::Object* source, vrt::Object* target)
  {
    static_cast<NodeFields*>(source->fields())->next = target->data();
  }

  void set_outgoing(vrt::Object* source, vrt::Object* target)
  {
    static_cast<NodeFields*>(source->fields())->outgoing = target->data();
    target->field_ref_inc();
  }

  void publish_scc(
    vrt::Region* region,
    vrt::Object* representative,
    std::initializer_list<vrt::Object*> members,
    vrt::RC arc)
  {
    for (auto* member : members)
    {
      if (!region->remove(member))
        std::abort();
    }

    representative->set_location(vrt::Location::immutable());
    for (auto* member : members)
    {
      if (member != representative)
        member->set_location(vrt::Location::scc_ptr(representative));
    }

    representative->set_arc(arc);
    if (region->stack_dec(members.size()))
      std::abort();
  }
}

int main()
{
  vrt_runtime_init();
  vrt_program_init(&program);
  vrt::init_thread();

  const vrt::Function root_function{1, "root", nullptr};
  auto* root_frame = vrt_frame_enter(&root_function);
  auto* frame_region = root_frame->region;

  // A two-object cycle is reclaimed through releases to both the canonical
  // representative and a non-representative member.
  auto* first_target = new_target(frame_region, 10);
  auto* pair_region = vrt::Region::create(vrt::RegionType::rc);
  auto* pair_a = new_node(pair_region);
  auto* pair_b = new_node(pair_region);
  set_next(pair_a, pair_b);
  set_next(pair_b, pair_a);
  set_outgoing(pair_b, first_target);
  publish_scc(pair_region, pair_a, {pair_a, pair_b}, 2);
  if (
    (pair_a->location() != vrt::Location::immutable()) ||
    !pair_b->location().is_scc_ptr() || (pair_b->representative() != pair_a) ||
    (pair_a->get_arc() != 2) || (first_target->get_rc() != 2))
    return 1;

  pair_b->root_ref_dec();
  if ((pair_a->get_arc() != 1) || (first_target->get_rc() != 2))
    return 2;

  pair_a->root_ref_dec();
  if (
    (first_target->get_rc() != 1) ||
    (finalizer_calls.load(std::memory_order_relaxed) != 2))
    return 3;
  first_target->root_ref_dec();

  // A self-cycle suppresses its internal decrement while still dropping its
  // outgoing mutable edge exactly once.
  auto* self_target = new_target(frame_region, 20);
  auto* self_region = vrt::Region::create(vrt::RegionType::rc);
  auto* self = new_node(self_region);
  set_next(self, self);
  set_outgoing(self, self_target);
  publish_scc(self_region, self, {self}, 1);
  self->root_ref_dec();
  if (self_target->get_rc() != 1)
    return 4;
  self_target->root_ref_dec();

  // A three-object component owns one reference to another immutable
  // component. Collecting the first decrements, but does not collect, the
  // second; releasing its remaining external reference then reclaims it.
  auto* chained_target = new_target(frame_region, 30);
  auto* sink_region = vrt::Region::create(vrt::RegionType::rc);
  auto* sink = new_node(sink_region);
  set_outgoing(sink, chained_target);
  publish_scc(sink_region, sink, {sink}, 2);

  auto* triple_region = vrt::Region::create(vrt::RegionType::rc);
  auto* triple_a = new_node(triple_region);
  auto* triple_b = new_node(triple_region);
  auto* triple_c = new_node(triple_region);
  set_next(triple_a, triple_b);
  set_next(triple_b, triple_c);
  set_next(triple_c, triple_a);
  static_cast<NodeFields*>(triple_a->fields())->outgoing = sink->data();
  publish_scc(triple_region, triple_a, {triple_a, triple_b, triple_c}, 1);
  if (
    (triple_b->representative() != triple_a) ||
    (triple_c->representative() != triple_a))
    return 5;

  triple_a->root_ref_dec();
  if ((sink->get_arc() != 1) || (chained_target->get_rc() != 2))
    return 6;

  sink->root_ref_dec();
  if (chained_target->get_rc() != 1)
    return 7;
  chained_target->root_ref_dec();

  // Concurrent retainers race through both a representative and an SCC_PTR
  // member. Only the one-to-zero releasing thread may collect both members and
  // drop the non-representative member's outgoing edge.
  auto* concurrent_target = new_target(frame_region, 40);
  auto* concurrent_region = vrt::Region::create(vrt::RegionType::rc);
  auto* concurrent_a = new_node(concurrent_region);
  auto* concurrent_b = new_node(concurrent_region);
  set_next(concurrent_a, concurrent_b);
  set_next(concurrent_b, concurrent_a);
  set_outgoing(concurrent_b, concurrent_target);
  constexpr vrt::RC concurrent_references = 8;
  publish_scc(
    concurrent_region, concurrent_a, {concurrent_a, concurrent_b}, 1);

  std::vector<std::thread> threads;
  threads.reserve(concurrent_references);
  std::atomic<vrt::RC> ready{0};
  std::atomic<bool> start{false};
  for (vrt::RC index = 1; index < concurrent_references; index++)
  {
    threads.emplace_back([&, index]() {
      ready.fetch_add(1, std::memory_order_relaxed);
      while (!start.load(std::memory_order_acquire))
        std::this_thread::yield();
      (index % 2 == 0 ? concurrent_a : concurrent_b)->root_ref_inc();
    });
  }

  while (ready.load(std::memory_order_relaxed) != concurrent_references - 1)
    std::this_thread::yield();
  start.store(true, std::memory_order_release);

  for (auto& thread : threads)
    thread.join();

  if (concurrent_a->get_arc() != concurrent_references)
    return 8;

  const auto calls_before_concurrent =
    finalizer_calls.load(std::memory_order_relaxed);
  threads.clear();
  ready.store(0, std::memory_order_relaxed);
  start.store(false, std::memory_order_relaxed);
  for (vrt::RC index = 0; index < concurrent_references; index++)
  {
    threads.emplace_back([&, index]() {
      vrt::init_thread();
      ready.fetch_add(1, std::memory_order_relaxed);
      while (!start.load(std::memory_order_acquire))
        std::this_thread::yield();
      (index % 2 == 0 ? concurrent_a : concurrent_b)->root_ref_dec();
      vrt::deinit_thread();
    });
  }

  while (ready.load(std::memory_order_relaxed) != concurrent_references)
    std::this_thread::yield();
  start.store(true, std::memory_order_release);

  for (auto& thread : threads)
    thread.join();

  if (
    (concurrent_target->get_rc() != 1) ||
    (finalizer_calls.load(std::memory_order_relaxed) !=
     calls_before_concurrent + 2))
    return 9;
  concurrent_target->root_ref_dec();

  if (frame_region->header_count() != 0)
    return 10;

  vrt_frame_leave();
  vrt::deinit_thread();
  return vrt_thread_current() == nullptr ? 0 : 11;
}
