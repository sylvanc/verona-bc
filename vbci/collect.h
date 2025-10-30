// Delay deallocation to handle re-entrancy.

namespace vbci
{
  template<typename T> void collect(T* h);
} // namespace vbci