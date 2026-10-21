// Test lambda capturing a mutable variable (var) as a free variable.
// Captured vars use a shared managed cell, including after a lambda escapes.

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

// Sibling closures share the same captured var.
make_shared_counter(): (() -> i32, () -> i32)
{
  var count: i32 = 0;
  let inc = (): i32 -> { count = count + 1; count };
  let get = (): i32 -> count;
  (inc, get)
}

// The managed cell outlives the frame that declared the var.
make_escaping_counter(): () -> i32
{
  var count: i32 = 0;
  (): i32 -> { count = count + 1; count }
}

// A managed cell can contain an enclosing type parameter.
make_generic_cell[T](value: T): (T -> T, () -> T)
{
  var cell: T = value;
  let set = (next: T): T -> { cell = next };
  let get = (): T -> cell;
  (set, get)
}

main(): none
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

  (let inc, let get) = lambda_var_capture::make_shared_counter();
  let shared1 = inc();
  let shared2 = get();
  let shared3 = inc();
  let shared4 = get();
  if (shared1 != 1) | (shared2 != 1) | (shared3 != 2) | (shared4 != 2)
  {
    result = result + 16
  }

  let escaping = lambda_var_capture::make_escaping_counter();
  let escape1 = escaping();
  let escape2 = escaping();
  if (escape1 != 1) | (escape2 != 2)
  {
    result = result + 32
  }

  (let set, let get_generic) =
    lambda_var_capture::make_generic_cell[i32](10);
  set(42);
  if get_generic() != 42
  {
    result = result + 64
  }

  ffi::exit_code result
}
