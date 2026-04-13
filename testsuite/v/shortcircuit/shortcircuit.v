// Test: short-circuiting boolean operators & and |.
// bool's & and | take a `to_bool` shape (anything with apply(): bool).
// A `counter` class satisfies to_bool: apply returns its stored bool value
// and increments its count field. After short-circuit operations, we check
// that count reflects whether the RHS was actually evaluated.
use
{
  printval = "printval"(any): none;
}

counter
{
  val: bool;
  count: i32;

  create(val: bool): counter
  {
    new {val, count = 0}
  }

  apply(self: counter): bool
  {
    self.count = self.count + 1;
    self.val
  }
}

main(): none
{
  // & tests: false & x should NOT evaluate x, true & x should evaluate x.
  let tt = counter(true);
  let tf = counter(false);
  let ft = counter(true);
  let ff = counter(false);

  let a = true & tt;   // evaluates tt -> true
  let b = true & tf;   // evaluates tf -> false
  let c = false & ft;  // short-circuits, does NOT evaluate ft
  let d = false & ff;  // short-circuits, does NOT evaluate ff

  // | tests: true | x should NOT evaluate x, false | x should evaluate x.
  let ot = counter(true);
  let of = counter(false);
  let pt = counter(true);
  let pf = counter(false);

  let e = true | ot;   // short-circuits, does NOT evaluate ot
  let f = true | of;   // short-circuits, does NOT evaluate of
  let g = false | pt;  // evaluates pt -> true
  let h = false | pf;  // evaluates pf -> false

  // Encode boolean results as a bitmask: abcdefgh
  // Expected: a=1 b=0 c=0 d=0 e=1 f=1 g=1 h=0
  // = 0b10001110 = 142
  var result = 0;
  if a { result = result + 128 }
  if b { result = result + 64 }
  if c { result = result + 32 }
  if d { result = result + 16 }
  if e { result = result + 8 }
  if f { result = result + 4 }
  if g { result = result + 2 }
  if h { result = result + 1 }

  :::printval(result);

  // Check evaluation counts.
  // & tests: tt and tf were evaluated (count=1), ft and ff were not (count=0).
  // | tests: ot and of were not evaluated (count=0), pt and pf were (count=1).
  var counts = tt.count + tf.count + ft.count + ff.count
    + ot.count + of.count + pt.count + pf.count;

  // Expected: tt=1 tf=1 ft=0 ff=0 ot=0 of=0 pt=1 pf=1 -> total=4
  :::printval(counts);
}
