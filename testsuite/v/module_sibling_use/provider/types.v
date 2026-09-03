// Fixture role: define `list::value` for the source-ordered sibling-use test.
// Failure mode: the parent test has no distinguishable value to prove that
// `list` was resolved through the preceding `provider` import.
// Assumptions: this file is loaded as the `provider` sibling module.

list
{
  value(): i32
  {
    42
  }
}
