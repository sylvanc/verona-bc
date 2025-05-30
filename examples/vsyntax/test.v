i32 {}

Class0[T1: none = i32, T2: bool = (f32 | f64) | (i32 | i64)]
{
  _f: i32 = 8;

  f1() {}

  f[T3: i32 = i32](a: T3 = 1, b: T1): (T3 & (none | i32), bool)
  {
    use std::builtin;
    use Func1 = (i32->i32)->T1;
    use Func2 = i32->i32->bool;

    if a (b: T1) -> (a, b, true, 0b01);

    (while true {}; a);

    while (a + b)
    {
      a + b;
      (a + b; break);
      continue;
      return 5;
      raise;
      throw "Error";
    }

    for container (key, value) ->
    {
      key == value;
    }

    match x
    {
      (z) -> 3;
      (y: f64) -> { a / b; 4 }
    }

    a
  }
}


// t1: Node = Node(SomeToken);
// t2 = Node(SomeToken);

// Node
// {
//   _type: Token;
//   _location: Location;
//   _symtab: SymbolTable;
//   _parent: OptNode;
//   _flags: Flags;
//   _children: Array[Node];

//   create(type: Token): Node
//   {
//     new (type, Location, SymbolTable, None, Flags::none(), Array[Node]);
//   }

//   type(self): Token
//   {
//     self._type;
//   }

//   in(self, types: Array[Token]): Bool
//   {
//     for types
//     {
//       type =>
//       if (self._type == type)
//       {
//         return true;
//       }
//     }

//     false;
//   }
// }
