include_guard(GLOBAL)

function(verona_should_register_run out test)
  set(register TRUE)
  if(
    test MATCHES "(^|/)compile_only/"
    OR test MATCHES "^vir/vrt_[^/]+/vrt_[^/]+\\.vir$")
    set(register FALSE)
  endif()

  set(${out} "${register}" PARENT_SCOPE)
endfunction()
