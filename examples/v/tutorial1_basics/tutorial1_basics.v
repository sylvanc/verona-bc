// Tutorial 1: Hello World & Basics
//
// Every Verona program needs a main() returning i32.
// The return value is the process exit code.

// Functions are top-level declarations.
// The last expression in a block is its return value.
abs(x: i32): i32
{
  if x < 0 { -x } else { x }
}

main(): i32
{
  // let = immutable binding, var = mutable binding
  var sum: i32 = 0;

  // while loop
  var i: i32 = 1;
  while i <= 10
  {
    sum = sum + i;
    i = i + 1
  }
  // sum is now 55 (1+2+...+10)

  // if/else is an expression — it returns a value
  let label = if sum > 50 { 1 } else { 0 };

  // Calling our function
  let d = tutorial1_basics::abs(-3);

  // IMPORTANT: all operators have equal precedence!
  // a + b * c  means  (a + b) * c  — always use parens
  let check = d + (label * 0);

  // Verify: sum=55, label=1, d=3, check=3
  var result = 0;
  if sum != 55 { result = result + 1 }
  if label != 1 { result = result + 2 }
  if check != 3 { result = result + 4 }

  // 0 means all checks passed
  result
}
