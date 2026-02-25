main(): i32
{
  let s = "hello";
  var result = 0;

  // size
  if s.size != 5 { result = result + 1 }

  // indexing
  if s(1) != 'e' { result = result + 2 }

  // bool
  if !s.bool { result = result + 4 }
  if "".bool { result = result + 8 }

  // equality
  if s != "hello" { result = result + 16 }
  if s == "world" { result = result + 32 }

  // less than
  if !("abc" < "abd") { result = result + 64 }
  if "abd" < "abc" { result = result + 128 }
  if !("abc" < "abcd") { result = result + 256 }
  if "abcd" < "abc" { result = result + 512 }

  // greater than / less equal / greater equal
  if !("b" > "a") { result = result + 1024 }
  if !("a" <= "a") { result = result + 2048 }
  if !("a" >= "a") { result = result + 4096 }

  // concatenation
  let ab = "he" + "llo";
  if ab != "hello" { result = result + 8192 }
  if ab.size != 5 { result = result + 16384 }

  // empty string edge cases
  if "" != "" { result = result + 32768 }
  if !("" < "a") { result = result + 65536 }
  if "a" < "" { result = result + 131072 }
  if "abc" < "abc" { result = result + 262144 }
  if ("" + "hello") != "hello" { result = result + 524288 }
  if ("hello" + "") != "hello" { result = result + 1048576 }

  result
}
