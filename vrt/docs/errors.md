# Errors

`vrt::Error` defines stable runtime error identities for allocation, indexing,
stores, dispatch, stack escape, region entry points, freeze, merge, and
scheduler state. `vrt_error_message()` returns stable diagnostic text.

`vrt_try_invoke()` establishes an invocation boundary and returns either normal
completion or `ErrorInfo` containing the error, active generated function, and
optional site. `vrt_error_raise()` abandons the current generated invocation;
execution cannot resume at the failing operation.