// Test: lambdas may omit param types and return types — they're inferred
// from context. The strict check on Function nodes does not apply to lambdas.

main(): none
{
  let f = (x: i32) -> { x + 1 };
  let g = (y: i32) -> { y };
  let _ = f(i32 1)
}
