// Test var capture in nested lambdas.
// When an outer lambda is desugared by the sugar pass (topdown), the inner
// lambda must still find the outer lambda's var locals via symtab lookup.
// Regression test: the sugar pass must rebind locals in the generated apply
// function so that nested lambdas see them as Var (not Let).

_box { val: i32; }

main(): none
{
  var result = 0;

  // Test 1: var capture across nested lambda boundary.
  let test1 = () ->
  {
    var sum: i32 = 0;
    let f = (x: i32) -> { sum = sum + x };
    f 10;
    f 20;
    f 30;
    sum
  };
  if test1() != 60 { result = result + 1 }

  // Test 2: var write through nested lambda.
  let test2 = () ->
  {
    var x: i32 = 0;
    let set = (v: i32) -> { x = v };
    set 99;
    x
  };
  if test2() != 99 { result = result + 2 }

  // Test 3: multiple vars captured by one inner lambda.
  let test3 = () ->
  {
    var a: i32 = 0;
    var b: i32 = 0;
    let f = (x: i32) -> { a = a + x; b = b + 1 };
    f 10;
    f 20;
    a + b
  };
  if test3() != 32 { result = result + 4 }

  ffi::exit_code result
}
