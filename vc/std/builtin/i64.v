i64
{
  create(some: u64): i64
  {
    :::convi64(some)
  }

  +(self: i64, other: i64): i64
  {
    :::add(self, other)
  }

  -(self: i64, other: i64): i64
  {
    :::sub(self, other)
  }

  *(self: i64, other: i64): i64
  {
    :::mul(self, other)
  }

  /(self: i64, other: i64): i64
  {
    :::div(self, other)
  }

  %(self: i64, other: i64): i64
  {
    :::mod(self, other)
  }

  &(self: i64, other: i64): i64
  {
    :::and(self, other)
  }

  |(self: i64, other: i64): i64
  {
    :::or(self, other)
  }

  ^(self: i64, other: i64): i64
  {
    :::xor(self, other)
  }

  <<(self: i64, other: i64): i64
  {
    :::shl(self, other)
  }

  >>(self: i64, other: i64): i64
  {
    :::shr(self, other)
  }

  ==(self: i64, other: i64): bool
  {
    :::eq(self, other)
  }

  !=(self: i64, other: i64): bool
  {
    :::ne(self, other)
  }

  <(self: i64, other: i64): bool
  {
    :::lt(self, other)
  }

  <=(self: i64, other: i64): bool
  {
    :::le(self, other)
  }

  >(self: i64, other: i64): bool
  {
    :::gt(self, other)
  }

  >=(self: i64, other: i64): bool
  {
    :::ge(self, other)
  }

  min(self: i64, other: i64): i64
  {
    :::min(self, other)
  }

  max(self: i64, other: i64): i64
  {
    :::max(self, other)
  }

  -(self: i64): i64
  {
    :::neg(self)
  }

  !(self: i64): bool
  {
    :::not(self)
  }

  abs(self: i64): i64
  {
    :::abs(self)
  }
}
