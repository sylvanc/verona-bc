u8
{
  create(some: u64): u8
  {
    :::convu8(some)
  }

  +(self: u8, other: u8): u8
  {
    :::add(self, other)
  }

  -(self: u8, other: u8): u8
  {
    :::sub(self, other)
  }

  *(self: u8, other: u8): u8
  {
    :::mul(self, other)
  }

  /(self: u8, other: u8): u8
  {
    :::div(self, other)
  }

  %(self: u8, other: u8): u8
  {
    :::mod(self, other)
  }

  &(self: u8, other: u8): u8
  {
    :::and(self, other)
  }

  |(self: u8, other: u8): u8
  {
    :::or(self, other)
  }

  ^(self: u8, other: u8): u8
  {
    :::xor(self, other)
  }

  <<(self: u8, other: u8): u8
  {
    :::shl(self, other)
  }

  >>(self: u8, other: u8): u8
  {
    :::shr(self, other)
  }

  ==(self: u8, other: u8): bool
  {
    :::eq(self, other)
  }

  !=(self: u8, other: u8): bool
  {
    :::ne(self, other)
  }

  <(self: u8, other: u8): bool
  {
    :::lt(self, other)
  }

  <=(self: u8, other: u8): bool
  {
    :::le(self, other)
  }

  >(self: u8, other: u8): bool
  {
    :::gt(self, other)
  }

  >=(self: u8, other: u8): bool
  {
    :::ge(self, other)
  }

  min(self: u8, other: u8): u8
  {
    :::min(self, other)
  }

  max(self: u8, other: u8): u8
  {
    :::max(self, other)
  }

  -(self: u8): u8
  {
    :::neg(self)
  }

  !(self: u8): bool
  {
    :::not(self)
  }
}
