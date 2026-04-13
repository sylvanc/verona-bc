main(): none
{
  var result = 0;

  // Value match: matching arm.
  let a: i32 = (match i32 42 { (i32 42) -> i32 1; }) else (i32 0);
  if !(a == 1) { result = result + 1 }

  // Value match: non-matching arm falls through to else.
  let b: i32 = (match i32 10 { (i32 42) -> i32 1; }) else (i32 0);
  if !(b == 0) { result = result + 2 }

  // Value match with type match: value checked first.
  let c: i32 = (match i32 7 { (i32 7) -> i32 100; (x: i32) -> x; }) else (i32 0);
  if !(c == 100) { result = result + 4 }

  // Value match with type match: value doesn't match, type does.
  let d: i32 = (match i32 7 { (i32 42) -> i32 100; (x: i32) -> x; }) else (i32 0);
  if !(d == 7) { result = result + 8 }

  // Multiple value matches.
  let e: i32 = (match i32 2 { (i32 1) -> i32 10; (i32 2) -> i32 20; (i32 3) -> i32 30; }) else (i32 0);
  if !(e == 20) { result = result + 16 }

  // No value matches, else taken.
  let f: i32 = (match i32 99 { (i32 1) -> i32 10; (i32 2) -> i32 20; }) else (i32 0);
  if !(f == 0) { result = result + 32 }

  ffi::exit_code result
}
