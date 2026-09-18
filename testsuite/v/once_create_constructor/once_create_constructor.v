// Tests: an explicit `once create` suppresses automatic RHS `create`
// synthesis and remains the target of zero-argument constructor sugar.
// Failure mode: `holder::create()` resolves to a synthesized constructor that
// produces `holder`, so `accept`'s runtime argument type check rejects it
// because `cown[holder]` is required.
// Assumptions: once functions take no parameters, and an RHS call may fall
// back to a matching once function when no RHS overload exists.

holder
{
  once create(): cown[holder]
  {
    cown(new {})
  }
}

accept(value: cown[holder]): none
{
}

main(): none
{
  once_create_constructor::accept(holder::create())
}
