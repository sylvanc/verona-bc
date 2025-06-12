isize
{
  create(some: u64): isize
  {
    :::convisize(some)
  }

  +(self: isize, other: isize): isize
  {
    :::add(self, other)
  }

  -(self: isize, other: isize): isize
  {
    :::sub(self, other)
  }

  *(self: isize, other: isize): isize
  {
    :::mul(self, other)
  }

  /(self: isize, other: isize): isize
  {
    :::div(self, other)
  }

  %(self: isize, other: isize): isize
  {
    :::mod(self, other)
  }

  &(self: isize, other: isize): isize
  {
    :::and(self, other)
  }

  |(self: isize, other: isize): isize
  {
    :::or(self, other)
  }

  ^(self: isize, other: isize): isize
  {
    :::xor(self, other)
  }

  <<(self: isize, other: isize): isize
  {
    :::shl(self, other)
  }

  >>(self: isize, other: isize): isize
  {
    :::shr(self, other)
  }

  ==(self: isize, other: isize): bool
  {
    :::eq(self, other)
  }

  !=(self: isize, other: isize): bool
  {
    :::ne(self, other)
  }

  <(self: isize, other: isize): bool
  {
    :::lt(self, other)
  }

  <=(self: isize, other: isize): bool
  {
    :::le(self, other)
  }

  >(self: isize, other: isize): bool
  {
    :::gt(self, other)
  }

  >=(self: isize, other: isize): bool
  {
    :::ge(self, other)
  }

  min(self: isize, other: isize): isize
  {
    :::min(self, other)
  }

  max(self: isize, other: isize): isize
  {
    :::max(self, other)
  }

  -(self: isize): isize
  {
    :::neg(self)
  }

  !(self: isize): bool
  {
    :::not(self)
  }

  abs(self: isize): isize
  {
    :::abs(self)
  }
}
