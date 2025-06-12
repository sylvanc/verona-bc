u16
{
  create(some: u64): u16
  {
    :::convu16(some)
  }

  +(self: u16, other: u16): u16
  {
    :::add(self, other)
  }

  -(self: u16, other: u16): u16
  {
    :::sub(self, other)
  }

  *(self: u16, other: u16): u16
  {
    :::mul(self, other)
  }

  /(self: u16, other: u16): u16
  {
    :::div(self, other)
  }

  %(self: u16, other: u16): u16
  {
    :::mod(self, other)
  }

  &(self: u16, other: u16): u16
  {
    :::and(self, other)
  }

  |(self: u16, other: u16): u16
  {
    :::or(self, other)
  }

  ^(self: u16, other: u16): u16
  {
    :::xor(self, other)
  }

  <<(self: u16, other: u16): u16
  {
    :::shl(self, other)
  }

  >>(self: u16, other: u16): u16
  {
    :::shr(self, other)
  }

  ==(self: u16, other: u16): bool
  {
    :::eq(self, other)
  }

  !=(self: u16, other: u16): bool
  {
    :::ne(self, other)
  }

  <(self: u16, other: u16): bool
  {
    :::lt(self, other)
  }

  <=(self: u16, other: u16): bool
  {
    :::le(self, other)
  }

  >(self: u16, other: u16): bool
  {
    :::gt(self, other)
  }

  >=(self: u16, other: u16): bool
  {
    :::ge(self, other)
  }

  min(self: u16, other: u16): u16
  {
    :::min(self, other)
  }

  max(self: u16, other: u16): u16
  {
    :::max(self, other)
  }

  -(self: u16): u16
  {
    :::neg(self)
  }

  !(self: u16): bool
  {
    :::not(self)
  }
}
