// Test: FuncType as a shape.
// A function type (i32 -> i32) becomes a shape with an apply method.
// Lambdas (which become classes with apply methods) should satisfy
// the function type shape.

apply_fn(f: i32 -> i32, x: i32): i32
{
  f(x)
}

main(): none
{
  let inc = (x: i32): i32 -> { x + 1 }
  ffi::exit_code(functype::apply_fn(inc, 41))
}
