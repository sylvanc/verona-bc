ilong
{
  create(some: u64): ilong
  {
    :::convilong(some)
  }

  +(self: ilong, other: ilong): ilong
  {
    :::add(self, other)
  }

  -(self: ilong, other: ilong): ilong
  {
    :::sub(self, other)
  }

  *(self: ilong, other: ilong): ilong
  {
    :::mul(self, other)
  }

  /(self: ilong, other: ilong): ilong
  {
    :::div(self, other)
  }

  %(self: ilong, other: ilong): ilong
  {
    :::mod(self, other)
  }

  &(self: ilong, other: ilong): ilong
  {
    :::and(self, other)
  }

  |(self: ilong, other: ilong): ilong
  {
    :::or(self, other)
  }

  ^(self: ilong, other: ilong): ilong
  {
    :::xor(self, other)
  }

  <<(self: ilong, other: ilong): ilong
  {
    :::shl(self, other)
  }

  >>(self: ilong, other: ilong): ilong
  {
    :::shr(self, other)
  }

  ==(self: ilong, other: ilong): bool
  {
    :::eq(self, other)
  }

  !=(self: ilong, other: ilong): bool
  {
    :::ne(self, other)
  }

  <(self: ilong, other: ilong): bool
  {
    :::lt(self, other)
  }

  <=(self: ilong, other: ilong): bool
  {
    :::le(self, other)
  }

  >(self: ilong, other: ilong): bool
  {
    :::gt(self, other)
  }

  >=(self: ilong, other: ilong): bool
  {
    :::ge(self, other)
  }

  min(self: ilong, other: ilong): ilong
  {
    :::min(self, other)
  }

  max(self: ilong, other: ilong): ilong
  {
    :::max(self, other)
  }

  -(self: ilong): ilong
  {
    :::neg(self)
  }

  !(self: ilong): bool
  {
    :::not(self)
  }

  abs(self: ilong): ilong
  {
    :::abs(self)
  }
}
