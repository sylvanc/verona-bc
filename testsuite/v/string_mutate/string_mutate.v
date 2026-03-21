main(): i32
{
  var result = 0;

  // --- append ---
  var s = "hello".copy;
  s.append(" world");
  if s != "hello world" { result = result + 1 }
  if s.size != 11 { result = result + 2 }

  // --- clear ---
  var s2 = "abc".copy;
  s2.clear;
  if s2.size != 0 { result = result + 4 }
  if s2.bool { result = result + 8 }

  // --- insert ---
  var s3 = "helo".copy;
  s3.insert(3, "l");
  if s3 != "hello" { result = result + 16 }

  var s4 = "world".copy;
  s4.insert(0, "hello ");
  if s4 != "hello world" { result = result + 32 }

  // --- erase ---
  var s5 = "hello world".copy;
  s5.erase(5, 6);
  if s5 != "hello" { result = result + 64 }
  if s5.size != 5 { result = result + 128 }

  // --- replace ---
  var s6 = "hello world".copy;
  s6.replace(6, 5, "verona");
  if s6 != "hello verona" { result = result + 256 }

  // --- reserve + append ---
  var s7 = "a".copy;
  s7.reserve(100);
  s7.append("b");
  s7.append("c");
  if s7 != "abc" { result = result + 512 }
  if s7.size != 3 { result = result + 1024 }

  // --- substring preserves original ---
  var s8 = "hello world".copy;
  let sub = s8.substring(0, 5);
  if sub != "hello" { result = result + 2048 }
  if s8 != "hello world" { result = result + 4096 }

  result
}
