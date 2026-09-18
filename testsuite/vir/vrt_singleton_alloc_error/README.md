# VRT singleton allocation error fixture

**Coverage:** The native pipeline initializes an empty-class singleton, then
uses it as the locator for an RC heap allocation and must reject the invalid
allocation target.

**Native VRT coverage:** VRT diagnoses `runtime error: bad alloc target` and
terminates the native program with status 1.

**VBCI coverage:** The fixture is compiled to bytecode to validate the shared
VIR input, but VBCI execution is intentionally not registered because the
allocation error is specific to VRT.

**Non-goals:** The fixture does not compare VRT and VBCI diagnostics or cover
other allocation errors.