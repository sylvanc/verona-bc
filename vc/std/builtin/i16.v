i16
{
  create(some: u64): i16
  {
    :::convi16(some)
  }

  +(self: i16, other: i16): i16
  {
    :::add(self, other)
  }

  -(self: i16, other: i16): i16
  {
    :::sub(self, other)
  }

  *(self: i16, other: i16): i16
  {
    :::mul(self, other)
  }

  /(self: i16, other: i16): i16
  {
    :::div(self, other)
  }

  %(self: i16, other: i16): i16
  {
    :::mod(self, other)
  }

  &(self: i16, other: i16): i16
  {
    :::and(self, other)
  }

  |(self: i16, other: i16): i16
  {
    :::or(self, other)
  }

  ^(self: i16, other: i16): i16
  {
    :::xor(self, other)
  }

  <<(self: i16, other: i16): i16
  {
    :::shl(self, other)
  }

  >>(self: i16, other: i16): i16
  {
    :::shr(self, other)
  }

  ==(self: i16, other: i16): bool
  {
    :::eq(self, other)
  }

  !=(self: i16, other: i16): bool
  {
    :::ne(self, other)
  }

  <(self: i16, other: i16): bool
  {
    :::lt(self, other)
  }

  <=(self: i16, other: i16): bool
  {
    :::le(self, other)
  }

  >(self: i16, other: i16): bool
  {
    :::gt(self, other)
  }

  >=(self: i16, other: i16): bool
  {
    :::ge(self, other)
  }

  min(self: i16, other: i16): i16
  {
    :::min(self, other)
  }

  max(self: i16, other: i16): i16
  {
    :::max(self, other)
  }

  -(self: i16): i16
  {
    :::neg(self)
  }

  !(self: i16): bool
  {
    :::not(self)
  }

  abs(self: i16): i16
  {
    :::abs(self)
  }
}
