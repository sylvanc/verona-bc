add_executable(vbcc
  passes/assignids.cc
  passes/labels.cc
  passes/parser.cc
  passes/statements.cc
  bytecode.cc
  lang.cc
  main.cc
)

if(MSVC)
  target_compile_options(vbcc PRIVATE /W4 /WX /wd5030 /bigobj)
else()
  target_compile_options(vbcc PRIVATE
    -Wall -Wextra -Werror -Wshadow -Wno-zero-length-array)
endif()

target_link_libraries(vbcc
  Threads::Threads
  snmalloc-new-override
  CLI11::CLI11
  verona-bc::include
  trieste::trieste
)

install(TARGETS vbcc RUNTIME DESTINATION vbcc)
