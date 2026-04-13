use bool_thunk = ()->bool;

bool
{
  create(some: bool = false): bool
  {
    some
  }

  apply(self: bool): bool
  {
    self
  }

  &(self: bool, other: bool_thunk): bool
  {
    if self { other() } else { false }
  }

  |(self: bool, other: bool_thunk): bool
  {
    if self { true } else { other() }
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

  bool(self: bool): bool
  {
    self
  }

  string(self: bool): string
  {
    if self { "true" } else { "false" }
  }
}
