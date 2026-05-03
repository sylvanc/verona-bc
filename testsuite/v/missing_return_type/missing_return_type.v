// Test: structure pass rejects user-source functions missing a return type.
// Lambdas are exempt (they always allow inference).

foo(x: i32) { x + 1 }

main(): none
{
  let r: i32 = foo(i32 1)
}
