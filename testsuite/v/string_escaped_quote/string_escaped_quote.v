main(): none
{
  var result = 0;

  let quote = "\"";
  if quote != "\"" { result = result + 1 }
  if quote.size != 1 { result = result + 2 }

  let s = "a\"b";
  if s != "a\"b" { result = result + 4 }
  if s.size != 3 { result = result + 8 }

  let wrapped = "\"quoted\"";
  if wrapped != "\"quoted\"" { result = result + 16 }
  if wrapped.size != 8 { result = result + 32 }

  ffi::exit_code result
}
