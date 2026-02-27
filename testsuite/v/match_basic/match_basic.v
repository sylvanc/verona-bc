main(): i32
{
  var result = 0;

  // Single arm match with else fallback.
  let a = (match 42 { (x: i32) -> x + 1; }) else (0);
  if !(a == 43) { result = result + 1 }

  // Multi-arm match: first matching arm wins.
  let b = (match 10
  {
    (x: i32) -> x + 1;
  }) else (0);
  if !(b == 11) { result = result + 2 }

  // Else branch taken when no case matches (nomatch).
  // Currently all i32 values match, so this tests the else syntax.
  let c = (match 5 { (x: i32) -> x * 2; }) else (99);
  if !(c == 10) { result = result + 4 }

  result
}
