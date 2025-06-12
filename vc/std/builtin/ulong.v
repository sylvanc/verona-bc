ulong
{
  create(some: u64): ulong
  {
    :::convulong(some)
  }

  +(self: ulong, other: ulong): ulong
  {
    :::add(self, other)
  }

  -(self: ulong, other: ulong): ulong
  {
    :::sub(self, other)
  }

  *(self: ulong, other: ulong): ulong
  {
    :::mul(self, other)
  }

  /(self: ulong, other: ulong): ulong
  {
    :::div(self, other)
  }

  %(self: ulong, other: ulong): ulong
  {
    :::mod(self, other)
  }

  &(self: ulong, other: ulong): ulong
  {
    :::and(self, other)
  }

  |(self: ulong, other: ulong): ulong
  {
    :::or(self, other)
  }

  ^(self: ulong, other: ulong): ulong
  {
    :::xor(self, other)
  }

  <<(self: ulong, other: ulong): ulong
  {
    :::shl(self, other)
  }

  >>(self: ulong, other: ulong): ulong
  {
    :::shr(self, other)
  }

  ==(self: ulong, other: ulong): bool
  {
    :::eq(self, other)
  }

  !=(self: ulong, other: ulong): bool
  {
    :::ne(self, other)
  }

  <(self: ulong, other: ulong): bool
  {
    :::lt(self, other)
  }

  <=(self: ulong, other: ulong): bool
  {
    :::le(self, other)
  }

  >(self: ulong, other: ulong): bool
  {
    :::gt(self, other)
  }

  >=(self: ulong, other: ulong): bool
  {
    :::ge(self, other)
  }

  min(self: ulong, other: ulong): ulong
  {
    :::min(self, other)
  }

  max(self: ulong, other: ulong): ulong
  {
    :::max(self, other)
  }

  -(self: ulong): ulong
  {
    :::neg(self)
  }

  !(self: ulong): bool
  {
    :::not(self)
  }
}
