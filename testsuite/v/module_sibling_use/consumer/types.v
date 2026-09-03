// Fixture role: import `provider`, then resolve `list` through that import and
// call `value` unqualified.
// Failure mode: lookup consults the current or a later `use` instead of the
// preceding include, so `list` or `value` cannot be resolved.
// Assumptions: `use` declarations are resolved in source order and included
// definitions participate in unqualified lookup.

use provider;
use list;

get(): i32
{
  value()
}
