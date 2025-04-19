#include "types.h"
#include "object.h"

namespace vbci
{
  void Class::calc_size()
  {
    size = sizeof(Object) + (fields.size() * sizeof(Value));
  }

  Function* Class::finalizer()
  {
    return method(FinalMethodId);
  }

  Function* Class::method(Id w)
  {
    auto find = methods.find(w);
    if (find == methods.end())
      return nullptr;

    return find->second;
  }
}
