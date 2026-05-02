// Regression test: vbcc liveness pass must mark NewArray's size operand as
// "use(Rhs)". Without this, when vbcc's optimize pass inlines a function
// containing NewArray (e.g. `array::alloc`), the inlined size register is
// dropped before NewArray reads it, producing a runtime "bad conversion".

main(): none
{
  let a = array[i32]::alloc(4);
  let b = array[i32]::alloc(8);

  var result = 0;
  if a.size != 4 { result = result + 1 }
  if b.size != 8 { result = result + 2 }
  ffi::exit_code(result)
}

