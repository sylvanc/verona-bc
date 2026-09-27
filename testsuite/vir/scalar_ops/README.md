# LLVM scalar operations fixture

This fixture verifies VIR scalar lowering through both the bytecode and native
LLVM pipelines. Runtime checks combine every mismatch into the process exit
code, while helper functions also compile scalar return representations.

## Coverage

- Constants for every VIR integer and floating-point representation.
- Signed integer arithmetic, including negative division and remainder.
- Boolean and integer bitwise operations, shifts, minimum, maximum, negation,
  absolute value, and complement.
- Equality and ordered comparisons for Boolean and signed integer values.
- Return values for each primitive representation.

## Native VRT coverage

VRT starts the generated program and records the accumulated result through
`set_exit_code`. The scalar operations themselves lower to native LLVM
instructions and do not consume VRT object metadata or allocation APIs.

## Non-goals

The fixture does not exercise floating-point arithmetic or comparisons,
overflow trapping, saturating arithmetic, conversions, or object values.