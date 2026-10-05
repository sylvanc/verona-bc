include_guard(GLOBAL)

function(verona_should_register_run out test)
  set(register TRUE)
  if(test MATCHES "(^|/)compile_only/")
    set(register FALSE)
  endif()

  set(${out} "${register}" PARENT_SCOPE)
endfunction()
