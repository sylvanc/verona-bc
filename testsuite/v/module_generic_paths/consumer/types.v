// Fixture role: exercise an imported generic constructor from inside another
// generic class while preserving the consumer's `T` binder.
// Failure mode: `item(value)` loses `T`, resolves outside `provider`, or
// returns an incompatible specialization.
// Assumptions: `use provider` permits unqualified lookup of `provider::item`.

use provider;

holder[T]
{
  make(self: holder[T], value: T): provider::item[T]
  {
    item(value)
  }
}
