// Tests: a generic class used in a function signature must spell all of its
// type arguments explicitly.
// Failure mode: the bare `box` parameter type is accepted and reaches later
// passes with an unresolved class argument.
// Assumptions: signatures are type positions, so generic omissions are errors
// rather than inferable call-path holes.

box[T]
{}

consume(value: box): none
{}

main(): none
{}
