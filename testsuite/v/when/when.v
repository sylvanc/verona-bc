use
{
  printval = "printval"(any): none;
}

cell
{
  f: i32;

  create(f: i32 = i32 0): cell
  {
    new {f}
  }
}

main(): i32
{
  // Create a cown holding cell(10).
  let a = when ()
  {
    cell(i32 10)
  }

  // Create another cown holding cell(20).
  let b = when ()
  {
    cell(i32 20)
  }

  // Read from a, double its value, produce a new cell.
  let c = when (a) (x) ->
  {
    let v = (*x).f;
    cell(v + v)
  }

  // Combine b and c: sum their fields.
  let d = when (b, c) (y, z) ->
  {
    let sum = (*y).f + (*z).f;
    :::printval(sum);
    cell(sum)
  }
  0
}
