#pragma once

#include "../include/vrt/region.h"

#include <cstddef>
#include <cstdint>

namespace vrt
{
  struct Class;
  struct Frame;
  struct Header;
  struct Object;

  /** Common ownership state and allocation interface for runtime regions. */
  struct Region
  {
    Region* parent = nullptr;
    Header* entry_point = nullptr;
    uintptr_t stack_reference_count = 0;
    uintptr_t frame_depth = 0;
    RegionType type;
    bool destroying = false;

  protected:
    Region(RegionType type, uintptr_t frame_depth)
    : frame_depth(frame_depth), type(type)
    {}

  public:
    virtual ~Region() = default;
    static Region* create(RegionType type, uintptr_t frame_depth = 0);

    virtual Object* object(const Class* cls) = 0;
    virtual void insert(Header* header) = 0;
    virtual bool remove(Header* header) = 0;
    virtual bool contains(Header* header) const = 0;
    virtual size_t header_count() const = 0;
    virtual bool is_finalizing() const = 0;
    virtual bool begin_finalizing() = 0;
    virtual void finalize_contents() = 0;
    virtual void release_dead_objects() = 0;

    bool is_frame_local() const;
    virtual bool is_arena() const;
    bool has_parent() const;
    bool is_ancestor_of(const Region* other) const;

    void stack_inc(uintptr_t amount = 1);
    bool stack_dec(uintptr_t amount = 1);
    void set_parent(Region* new_parent, Header* entry);
    void clear_parent();
  };

  Region* frame_region(Frame* frame);
  Region* current_frame_region();
  void destroy_region(Region* region);
  void destroy_frame_region(Frame* frame);
}
