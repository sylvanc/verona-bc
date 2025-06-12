usize
{
  create(some: u64): usize
  {
    :::convusize(some)
  }

  +(self: usize, other: usize): usize
  {
    :::add(self, other)
  }

  -(self: usize, other: usize): usize
  {
    :::sub(self, other)
  }

  *(self: usize, other: usize): usize
  {
    :::mul(self, other)
  }

  /(self: usize, other: usize): usize
  {
    :::div(self, other)
  }

  %(self: usize, other: usize): usize
  {
    :::mod(self, other)
  }

  &(self: usize, other: usize): usize
  {
    :::and(self, other)
  }

  |(self: usize, other: usize): usize
  {
    :::or(self, other)
  }

  ^(self: usize, other: usize): usize
  {
    :::xor(self, other)
  }

  <<(self: usize, other: usize): usize
  {
    :::shl(self, other)
  }

  >>(self: usize, other: usize): usize
  {
    :::shr(self, other)
  }

  ==(self: usize, other: usize): bool
  {
    :::eq(self, other)
  }

  !=(self: usize, other: usize): bool
  {
    :::ne(self, other)
  }

  <(self: usize, other: usize): bool
  {
    :::lt(self, other)
  }

  <=(self: usize, other: usize): bool
  {
    :::le(self, other)
  }

  >(self: usize, other: usize): bool
  {
    :::gt(self, other)
  }

  >=(self: usize, other: usize): bool
  {
    :::ge(self, other)
  }

  min(self: usize, other: usize): usize
  {
    :::min(self, other)
  }

  max(self: usize, other: usize): usize
  {
    :::max(self, other)
  }

  -(self: usize): usize
  {
    :::neg(self)
  }

  !(self: usize): bool
  {
    :::not(self)
  }
}
