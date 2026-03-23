holder[A]
{
  c: cown[A];

  create(w: A): holder[A]
  {
    new {c = cown w}
  }

  send(self: holder): none
  {
    let msg = "hello";
    when self.c w ->
    {
      match msg
      {
        (s: string) -> (*w).tick;
      }
    }
  }
}

writer
{
  create(): holder[writer]
  {
    holder(new {})
  }

  tick(self: writer): none
  {
  }
}

main(): i32
{
  let a = writer();
  let tag = "out";
  match tag
  {
    (s: string) -> none;
  }
  a.send();
  0
}
