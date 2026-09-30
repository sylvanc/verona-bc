// Tests: using a generic type alias without its required type arguments is
// rejected during name resolution.
// Failure mode: the bare `identity` alias is accepted or silently treated as
// `any`, allowing `holder` to be constructed with an unresolved field type.
// Assumptions: aliases obey the same explicit-argument rule as generic classes
// when they appear in type positions.

use identity[T] = T;

holder
{
  value: identity;
}

main(): none
{
  let value = holder(none);
}
