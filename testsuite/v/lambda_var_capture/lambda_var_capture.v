// Test lambda capturing a mutable variable (var) as a free variable.
// Var-capturing lambdas are compiled as blocks (stack-allocated),
// using the same mechanism as raising lambdas.

// Lambda reads a captured var.
read_var(n: i32): i32
{
  var x: i32 = n;
  let f = (y: i32): i32 -> { x + y }
  f(0)
}

// Lambda writes to a captured var.
write_var(): i32
{
  var x: i32 = 0;
  let f = (): i32 -> { x = 42 }
  f();
  x
}

// Lambda captures a var AND raises.
var_and_raise(n: i32, target: i32): i32
{
  var x: i32 = n;
  let check = (t: i32) -> {
    if x == t { raise x }
  }
  check(target);
  0
}

main(): i32
{
  var result = 0;
  let a = lambda_var_capture::read_var(10);
  if a != 10 { result = result + 1 }
  let b = lambda_var_capture::write_var();
  if b != 42 { result = result + 2 }
  let c = lambda_var_capture::var_and_raise(42, 42);
  if c != 42 { result = result + 4 }
  let d = lambda_var_capture::var_and_raise(42, 99);
  if d != 0 { result = result + 8 }
  result
}
