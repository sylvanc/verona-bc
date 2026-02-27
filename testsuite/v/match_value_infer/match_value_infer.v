main(): i32
{
  var result = 0;
  let v: i32 = 42;

  // Value match: arm body is a typed expression, inference flows.
  let a = (match v { (42) -> v + 1; }) else (0);
  if !(a == 43) { result = result + 1 }

  // Value match: no match, else taken with inferred type.
  let b = (match v { (99) -> v; }) else (0);
  if !(b == 0) { result = result + 2 }

  // Multiple value arms: second arm matches.
  let c = (match v { (1) -> v + 10; (42) -> v + 20; (99) -> v + 30; }) else (0);
  if !(c == 62) { result = result + 4 }

  // Multiple value arms: none match, else taken.
  let d = (match v { (1) -> v; (2) -> v; (3) -> v; }) else (0);
  if !(d == 0) { result = result + 8 }

  // Mixed value + type: value matches first.
  let w: i32 = 7;
  let e = (match w { (7) -> w + 100; (x: i32) -> x; }) else (0);
  if !(e == 107) { result = result + 16 }

  // Mixed value + type: value doesn't match, type catches.
  let f = (match w { (42) -> w + 100; (x: i32) -> x; }) else (0);
  if !(f == 7) { result = result + 32 }

  result
}
