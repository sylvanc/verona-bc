// Test that local variables inside blocks and lambdas can shadow
// variables in the enclosing scope without corrupting the outer value.
// Also tests sibling-scope reuse and nested combinations.
main(): none
{
  // Outer x = 1
  let x: i32 = i32 1;

  // Shadow x inside an if block.
  if true
  {
    let x: i32 = i32 99;
    x
  }
  else
  {
    i32 0
  }

  // Outer x should still be 1.
  let a: i32 = x;

  // Shadow x inside a while block.
  while false
  {
    let x: i32 = i32 77;
    x
  }

  // Outer x should still be 1.
  let b: i32 = x;

  // Shadow x inside a lambda parameter.
  let f = (x: i32): i32 -> { x + i32 1 }
  f(i32 50);

  // Outer x should still be 1.
  let c: i32 = x;

  // Lambda nested inside control flow, shadowing x.
  let d: i32 = if true
  {
    let g = (x: i32): i32 -> { x + i32 2 }
    g(i32 60)
  }
  else
  {
    i32 0
  }

  // d = 62, outer x should still be 1.

  // Control flow inside a lambda, shadowing the lambda's own parameter.
  let h = (x: i32): i32 ->
  {
    let y: i32 = if true
    {
      let x: i32 = i32 5;
      x
    }
    else
    {
      i32 0
    }

    // x should still be the parameter value.
    x + y
  }
  let e: i32 = h(i32 100);

  // e = 100 + 5 = 105.

  // Lambda inside a lambda, both shadowing x.
  let outer = (x: i32): i32 ->
  {
    let inner = (x: i32): i32 -> { x + i32 3 }
    // x should still be the outer lambda's parameter.
    let r: i32 = inner(i32 200);
    x + r
  }
  let g: i32 = outer(i32 10);

  // g = 10 + 203 = 213.

  // Sibling scopes reusing x.
  let s: i32 = if true
  {
    let x: i32 = i32 7;
    x
  }
  else
  {
    let x: i32 = i32 8;
    x
  }

  // s = 7, outer x should still be 1.

  // a=1, b=1, c=1, d=62, e=105, g=213, s=7
  // sum = 1+1+1+62+105+213+7 = 390
  // exit code on Linux = 390 & 0xff = 134
  ffi::exit_code(a + b + c + d + e + g + s)
}
