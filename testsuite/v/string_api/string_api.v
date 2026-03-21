main(): i32
{
  var result = 0;

  // --- cstring ---
  let s = "hello";
  let cs = s.cstring;
  if cs.size != 6 { result = result + 1 }
  if cs(5) != 0 { result = result + 2 }

  // --- find ---
  let f1 = match s.find("ell")
  {
    (i: usize) -> i;
  }
  else
  {
    99
  };
  if f1 != 1 { result = result + 4 }

  let f2 = match s.find("xyz")
  {
    (i: usize) -> i;
  }
  else
  {
    99
  };
  if f2 != 99 { result = result + 8 }

  let f3 = match s.find("")
  {
    (i: usize) -> i;
  }
  else
  {
    99
  };
  if f3 != 0 { result = result + 16 }

  let f4 = match "".find("a")
  {
    (i: usize) -> i;
  }
  else
  {
    99
  };
  if f4 != 99 { result = result + 32 }

  // --- contains ---
  if !s.contains("llo") { result = result + 64 }
  if s.contains("xyz") { result = result + 128 }

  // --- starts_with / ends_with ---
  if !s.starts_with("hel") { result = result + 256 }
  if s.starts_with("elo") { result = result + 512 }
  if !s.ends_with("llo") { result = result + 1024 }
  if s.ends_with("hel") { result = result + 2048 }
  if !s.starts_with("") { result = result + 4096 }
  if !s.ends_with("") { result = result + 8192 }

  // --- substring ---
  let sub = s.substring(1, 3);
  if sub != "ell" { result = result + 16384 }
  if sub.size != 3 { result = result + 32768 }

  // --- trim ---
  let padded = "  hi  ";
  if padded.trim != "hi" { result = result + 65536 }
  if padded.trim_left != "hi  " { result = result + 131072 }
  if padded.trim_right != "  hi" { result = result + 262144 }
  if "".trim.size != 0 { result = result + 524288 }

  result
}
