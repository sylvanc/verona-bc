// Tests: generic paths resolve consistently through direct module names,
// unqualified imports, aliases, generated method calls, and nested classes.
// Failure mode: an import or alias resolves to the wrong class, a binder is
// dropped from a generated path, or nested `A`/`B` arguments are exchanged.
// Assumptions: `provider` and `consumer` are sibling module fixtures; `use`
// exposes imported definitions for unqualified lookup in source order.

use provider;
use p = provider;

main(): none
{
  var direct = provider::item(i32 11);
  var imported = item(i32 12);
  var aliased = p::item(i32 13);
  var h = consumer::holder[i32];
  var shared = h.make(14);
  var nested =
    provider::outer[u64]::inner[i32]::choose(u64 15, i32 1);

  var result = 0;
  if direct.get != 11 { result = result + 1 }
  if imported.get != 12 { result = result + 2 }
  if aliased.get != 13 { result = result + 4 }
  if shared.get != 14 { result = result + 8 }
  if nested != 15 { result = result + 16 }
  ffi::exit_code result
}
