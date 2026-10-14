// Coverage: Location size and traits, tag alignment and encodings, stack
// ordering, raw round trips, and Pending/SccPtr conversions.
// Representation coverage: direct tag construction verifies the encoding
// contract without requiring runtime-created objects.
// Non-goals: frame ownership and runtime immutable-state transitions are
// covered by the Frame, Freeze, SCC, and collector fixtures.

#include "location.h"

#include "header.h"
#include "region.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

static_assert(sizeof(vrt::Location) == sizeof(uintptr_t));
static_assert(std::is_trivially_copyable_v<vrt::Location>);
static_assert(!std::is_default_constructible_v<vrt::Location>);
static_assert(std::is_same_v<decltype(vrt::Header::reference_count), vrt::RC>);
static_assert(
  std::is_same_v<decltype(vrt::Region::stack_reference_count), vrt::RC>);
static_assert(
  alignof(vrt::Header) >= std::atomic_ref<vrt::RC>::required_alignment);
static_assert(alignof(vrt::Region) > vrt::Location::Mask);
static_assert(alignof(vrt::Header) > vrt::Location::Mask);

int main()
{
  auto root = vrt::Location::stack();
  auto child = root.next_stack_level();
  if (
    (root.raw() != 0x1) || (child.raw() != 0x9) || !root.is_stack() ||
    root.is_region() || (root.stack_index() != 0) ||
    (child.stack_index() != 1) || !(root < child) || !(root <= child) ||
    !(child > root) || !(child >= root) || (root == child) ||
    (vrt::Location::from_raw(child.raw()) != child) ||
    !vrt::Location::immutable().is_immutable() ||
    vrt::Location::immutable().is_scc_ptr() ||
    !vrt::Location::immortal().is_immortal() ||
    vrt::Location::immortal().is_immutable())
    return 1;

  alignas(vrt::Region) std::byte region_storage[sizeof(vrt::Region)];
  alignas(vrt::Header) std::byte header_storage[sizeof(vrt::Header)];
  auto* region = reinterpret_cast<vrt::Region*>(region_storage);
  auto* header = reinterpret_cast<vrt::Header*>(header_storage);
  auto region_location = vrt::Location(region);
  auto pending_location = region_location.pending();
  auto scc_location = vrt::Location::scc_ptr(header);
  if (
    !region_location.is_region() || (region_location.to_region() != region) ||
    (region_location.raw() != reinterpret_cast<uintptr_t>(region)) ||
    !pending_location.is_pending() || pending_location.is_region() ||
    (pending_location.unpending() != region_location) ||
    !scc_location.is_scc_ptr() || !scc_location.is_immutable() ||
    scc_location.is_region() || (scc_location.scc_target() != header) ||
    (vrt::Location::from_raw(scc_location.raw()) != scc_location))
    return 2;

  return 0;
}
