u64
{
  create(some: u64): u64
  {
    some
  }

  +(self: u64, other: u64): u64
  {
    :::add(self, other)
  }

  -(self: u64, other: u64): u64
  {
    :::sub(self, other)
  }

  *(self: u64, other: u64): u64
  {
    :::mul(self, other)
  }

  /(self: u64, other: u64): u64
  {
    :::div(self, other)
  }

  %(self: u64, other: u64): u64
  {
    :::mod(self, other)
  }

  &(self: u64, other: u64): u64
  {
    :::and(self, other)
  }

  |(self: u64, other: u64): u64
  {
    :::or(self, other)
  }

  ^(self: u64, other: u64): u64
  {
    :::xor(self, other)
  }

  <<(self: u64, other: u64): u64
  {
    :::shl(self, other)
  }

  >>(self: u64, other: u64): u64
  {
    :::shr(self, other)
  }

  ==(self: u64, other: u64): bool
  {
    :::eq(self, other)
  }

  !=(self: u64, other: u64): bool
  {
    :::ne(self, other)
  }

  <(self: u64, other: u64): bool
  {
    :::lt(self, other)
  }

  <=(self: u64, other: u64): bool
  {
    :::le(self, other)
  }

  >(self: u64, other: u64): bool
  {
    :::gt(self, other)
  }

  >=(self: u64, other: u64): bool
  {
    :::ge(self, other)
  }

  min(self: u64, other: u64): u64
  {
    :::min(self, other)
  }

  max(self: u64, other: u64): u64
  {
    :::max(self, other)
  }

  -(self: u64): u64
  {
    :::neg(self)
  }

  !(self: u64): bool
  {
    :::not(self)
  }
}
