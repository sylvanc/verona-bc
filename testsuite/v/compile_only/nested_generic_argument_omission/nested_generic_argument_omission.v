// Tests: omitted type arguments are rejected recursively inside a qualified
// generic path, not only on its final class element.
// Failure mode: `outer[list]` is accepted even though `list` requires its own
// type argument, leaving an unresolved nested inference hole.
// Assumptions: generic TypeName positions require explicit arguments; only
// call-path function arguments may be represented as inference holes.

list[T]
{}

outer[T]
{
  foo(): none
  {}
}

main(): none
{
  outer[list]::foo()
}
