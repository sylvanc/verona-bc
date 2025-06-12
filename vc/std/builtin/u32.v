u32
{
  create(some: u64): u32
  {
    :::convu32(some)
  }

  +(self: u32, other: u32): u32
  {
    :::add(self, other)
  }

  -(self: u32, other: u32): u32
  {
    :::sub(self, other)
  }

  *(self: u32, other: u32): u32
  {
    :::mul(self, other)
  }

  /(self: u32, other: u32): u32
  {
    :::div(self, other)
  }

  %(self: u32, other: u32): u32
  {
    :::mod(self, other)
  }

  &(self: u32, other: u32): u32
  {
    :::and(self, other)
  }

  |(self: u32, other: u32): u32
  {
    :::or(self, other)
  }

  ^(self: u32, other: u32): u32
  {
    :::xor(self, other)
  }

  <<(self: u32, other: u32): u32
  {
    :::shl(self, other)
  }

  >>(self: u32, other: u32): u32
  {
    :::shr(self, other)
  }

  ==(self: u32, other: u32): bool
  {
    :::eq(self, other)
  }

  !=(self: u32, other: u32): bool
  {
    :::ne(self, other)
  }

  <(self: u32, other: u32): bool
  {
    :::lt(self, other)
  }

  <=(self: u32, other: u32): bool
  {
    :::le(self, other)
  }

  >(self: u32, other: u32): bool
  {
    :::gt(self, other)
  }

  >=(self: u32, other: u32): bool
  {
    :::ge(self, other)
  }

  min(self: u32, other: u32): u32
  {
    :::min(self, other)
  }

  max(self: u32, other: u32): u32
  {
    :::max(self, other)
  }

  -(self: u32): u32
  {
    :::neg(self)
  }

  !(self: u32): bool
  {
    :::not(self)
  }
}
