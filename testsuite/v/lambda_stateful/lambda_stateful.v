// Test stateful escaping lambdas.
// Captured let variables become mutable fields, so writes inside
// the lambda mutate the lambda's own state across calls.

// A counter that increments each time it is called.
make_counter(): () -> i32
{
  let count: i32 = 0;
  (): i32 -> { count = count + 1; count }
}

// An accumulator that sums values across calls.
make_accumulator(): i32 -> i32
{
  let total: i32 = 0;
  (n: i32): i32 -> { total = total + n; total }
}

// Verify that two closures over the same source have independent state.
// Each lambda gets its own copy of x as a field.
independent_state(): i32
{
  let x: i32 = 0;
  let a = (): i32 -> { x = x + 1; x }
  let b = (): i32 -> { x = x + 10; x }
  // a's x: 0 → 1 → 2
  a();
  let a2 = a();
  // b's x: 0 → 10 → 20
  b();
  let b2 = b();
  a2 + b2
}

main(): i32
{
  var result = 0;

  // Counter: successive calls return 1, 2, 3.
  let c = lambda_stateful::make_counter();
  let c1 = c();
  let c2 = c();
  let c3 = c();
  if (c1 != 1) | (c2 != 2) | (c3 != 3)
  {
    result = result + 1
  }

  // Accumulator: 10, then 10+20=30, then 30+5=35.
  let a = lambda_stateful::make_accumulator();
  let a1 = a(10);
  let a2 = a(20);
  let a3 = a(5);
  if (a1 != 10) | (a2 != 30) | (a3 != 35)
  {
    result = result + 2
  }

  // Independent state: a2=2, b2=20, sum=22.
  let s = lambda_stateful::independent_state();
  if s != 22
  {
    result = result + 4
  }

  result
}
