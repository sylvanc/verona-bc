#include "type_encoding.h"

#include "../model/ids.h"

#include <cassert>
#include <vbc/format.h>

namespace virc::vbc_backend
{
  using namespace trieste;
  using namespace ::vbc;
  using namespace vir;

  // VIRC owns backend-neutral semantic IDs. The VBC format owns wire IDs
  // serialized into .vbc files and interpreted by VBCI. VBC emission requires
  // their numeric representations to agree without coupling either contract.
  static_assert(virc::MainFunctionId == ::vbc::MainFunctionId);
  static_assert(virc::FinalizerMethodId == ::vbc::FinalizerMethodId);
  static_assert(virc::CallbackMethodId == ::vbc::CallbackMethodId);
  static_assert(virc::DynamicTypeId == ::vbc::DynamicTypeId);
  static_assert(PrimitiveTypeCount == NumPrimitiveClasses);
  static_assert(+PrimitiveKind::None == +PrimitiveType::None);
  static_assert(+PrimitiveKind::Bool == +PrimitiveType::Bool);
  static_assert(+PrimitiveKind::I8 == +PrimitiveType::I8);
  static_assert(+PrimitiveKind::I16 == +PrimitiveType::I16);
  static_assert(+PrimitiveKind::I32 == +PrimitiveType::I32);
  static_assert(+PrimitiveKind::I64 == +PrimitiveType::I64);
  static_assert(+PrimitiveKind::U8 == +PrimitiveType::U8);
  static_assert(+PrimitiveKind::U16 == +PrimitiveType::U16);
  static_assert(+PrimitiveKind::U32 == +PrimitiveType::U32);
  static_assert(+PrimitiveKind::U64 == +PrimitiveType::U64);
  static_assert(+PrimitiveKind::ILong == +PrimitiveType::ILong);
  static_assert(+PrimitiveKind::ULong == +PrimitiveType::ULong);
  static_assert(+PrimitiveKind::ISize == +PrimitiveType::ISize);
  static_assert(+PrimitiveKind::USize == +PrimitiveType::USize);
  static_assert(+PrimitiveKind::F32 == +PrimitiveType::F32);
  static_assert(+PrimitiveKind::F64 == +PrimitiveType::F64);
  static_assert(+PrimitiveKind::Ptr == +PrimitiveType::Ptr);

  uleb<size_t> encode_region(Node node)
  {
    auto region = node / Region;

    if (region == RegionRC)
      return +RegionType::RegionRC;
    if (region == RegionArena)
      return +RegionType::RegionArena;

    assert(false);
    return size_t(-1);
  }

  void encode_type_table(ByteBuffer& output, const std::vector<TypeInfo>& types)
  {
    output << uleb(types.size());

    for (const auto& type : types)
    {
      switch (type.kind)
      {
        case TypeKind::Array:
          output << uleb(+TypeTag::Array);
          break;
        case TypeKind::Cown:
          output << uleb(+TypeTag::Cown);
          break;
        case TypeKind::Ref:
          output << uleb(+TypeTag::Ref);
          break;
        case TypeKind::Union:
          output << uleb(+TypeTag::Union) << uleb(type.elements.size());
          break;
        case TypeKind::Tuple:
          output << uleb(+TypeTag::Tuple) << uleb(type.elements.size());
          break;
      }

      for (auto element : type.elements)
        output << uleb(element);
    }
  }
}