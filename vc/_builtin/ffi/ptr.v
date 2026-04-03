ptr
{
  create(from: any = none): ptr
  {
    :::read(:::ptr(from))
  }

  ==(self: ptr, other: ptr): bool
  {
    :::eq(self, other)
  }

  !=(self: ptr, other: ptr): bool
  {
    :::ne(self, other)
  }
}
