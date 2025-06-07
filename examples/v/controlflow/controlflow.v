std
{
  builtin
  {
    i32 {}
  }
}

// f0(): i32
// {
//   1 else 2 else 3
// }

// f1(): i32
// {
//   if true
//   {
//     "true"
//   }
//   else if false
//   {
//     "false"
//   }
//   else
//   {
//     "hi"
//   }
// }

f2(): i32
{
  (3 + if false { 1 }) * 2
}
