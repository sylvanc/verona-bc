i8
{
  create(some: u64): i8
  {
    :::convi8(some)
  }

  +(self: i8, other: i8): i8
  {
    :::add(self, other)
  }

  -(self: i8, other: i8): i8
  {
    :::sub(self, other)
  }

  *(self: i8, other: i8): i8
  {
    :::mul(self, other)
  }

  /(self: i8, other: i8): i8
  {
    :::div(self, other)
  }

  %(self: i8, other: i8): i8
  {
    :::mod(self, other)
  }

  &(self: i8, other: i8): i8
  {
    :::and(self, other)
  }

  |(self: i8, other: i8): i8
  {
    :::or(self, other)
  }

  ^(self: i8, other: i8): i8
  {
    :::xor(self, other)
  }

  <<(self: i8, other: i8): i8
  {
    :::shl(self, other)
  }

  >>(self: i8, other: i8): i8
  {
    :::shr(self, other)
  }

  ==(self: i8, other: i8): bool
  {
    :::eq(self, other)
  }

  !=(self: i8, other: i8): bool
  {
    :::ne(self, other)
  }

  <(self: i8, other: i8): bool
  {
    :::lt(self, other)
  }

  <=(self: i8, other: i8): bool
  {
    :::le(self, other)
  }

  >(self: i8, other: i8): bool
  {
    :::gt(self, other)
  }

  >=(self: i8, other: i8): bool
  {
    :::ge(self, other)
  }

  min(self: i8, other: i8): i8
  {
    :::min(self, other)
  }

  max(self: i8, other: i8): i8
  {
    :::max(self, other)
  }

  -(self: i8): i8
  {
    :::neg(self)
  }

  !(self: i8): bool
  {
    :::not(self)
  }

  abs(self: i8): i8
  {
    :::abs(self)
  }
}
