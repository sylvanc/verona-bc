#pragma once

namespace vrt
{
  struct Header;
  struct Region;

  void collect(Header* header);
  void collect(Region* region);
  void collect_scc(Header* representative);
}