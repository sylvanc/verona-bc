bool
{
  create(some: bool): bool
  {
    some
  }

  &(self: bool, other: bool): bool
  {
    :::and(self, other)
  }

  |(self: bool, other: bool): bool
  {
    :::or(self, other)
  }

  ^(self: bool, other: bool): bool
  {
    :::xor(self, other)
  }

  ==(self: bool, other: bool): bool
  {
    :::eq(self, other)
  }

  !=(self: bool, other: bool): bool
  {
    :::ne(self, other)
  }

  <(self: bool, other: bool): bool
  {
    :::lt(self, other)
  }

  <=(self: bool, other: bool): bool
  {
    :::le(self, other)
  }

  >(self: bool, other: bool): bool
  {
    :::gt(self, other)
  }

  >=(self: bool, other: bool): bool
  {
    :::ge(self, other)
  }

  min(self: bool, other: bool): bool
  {
    :::min(self, other)
  }

  max(self: bool, other: bool): bool
  {
    :::max(self, other)
  }

  !(self: bool): bool
  {
    :::not(self)
  }
}
