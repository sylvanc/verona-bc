cell
{
  f: i32;
  create(f: i32): cell { new {f} }
  print(self: cell, s: string): none { }
}

box[A]
{
  c: cown[A];

  create(a: A): box[A]
  {
    new {c = cown a}
  }

  get(self: box): none
  {
    when self.c w ->
    {
      (*w).f
    }
  }

  doit(self: box, s: string): none
  {
    when self.c w ->
    {
      match s
      {
        (x: string) -> (*w).print(x);
      }
    }
  }
}

main(): none
{
  let b = box(cell(i32 42));
  b.get;
  b.doit "hello";
}
