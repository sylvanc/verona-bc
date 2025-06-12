i32
{
  create(some: u64): i32
  {
    :::convi32(some)
  }

  +(self: i32, other: i32): i32
  {
    :::add(self, other)
  }

  -(self: i32, other: i32): i32
  {
    :::sub(self, other)
  }

  *(self: i32, other: i32): i32
  {
    :::mul(self, other)
  }

  /(self: i32, other: i32): i32
  {
    :::div(self, other)
  }

  %(self: i32, other: i32): i32
  {
    :::mod(self, other)
  }

  &(self: i32, other: i32): i32
  {
    :::and(self, other)
  }

  |(self: i32, other: i32): i32
  {
    :::or(self, other)
  }

  ^(self: i32, other: i32): i32
  {
    :::xor(self, other)
  }

  <<(self: i32, other: i32): i32
  {
    :::shl(self, other)
  }

  >>(self: i32, other: i32): i32
  {
    :::shr(self, other)
  }

  ==(self: i32, other: i32): bool
  {
    :::eq(self, other)
  }

  !=(self: i32, other: i32): bool
  {
    :::ne(self, other)
  }

  <(self: i32, other: i32): bool
  {
    :::lt(self, other)
  }

  <=(self: i32, other: i32): bool
  {
    :::le(self, other)
  }

  >(self: i32, other: i32): bool
  {
    :::gt(self, other)
  }

  >=(self: i32, other: i32): bool
  {
    :::ge(self, other)
  }

  min(self: i32, other: i32): i32
  {
    :::min(self, other)
  }

  max(self: i32, other: i32): i32
  {
    :::max(self, other)
  }

  -(self: i32): i32
  {
    :::neg(self)
  }

  !(self: i32): bool
  {
    :::not(self)
  }

  abs(self: i32): i32
  {
    :::abs(self)
  }
}
