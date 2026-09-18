# LLVM runtime allocation error fixture

**Coverage:** The native pipeline attempts to use an empty-class singleton as
the locator for an RC heap allocation and must reject the invalid allocation
target.

**Native VRT coverage:** VRT diagnoses `runtime error: bad alloc target` and
terminates the native program with status 1.

**Non-goals:** The fixture does not require the bytecode interpreter to use the
same diagnostic or exit status, and it does not cover other allocation errors.