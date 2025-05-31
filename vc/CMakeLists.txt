add_executable(vc
  passes/application.cc
  passes/parser.cc
  passes/structure.cc
  lang.cc
  main.cc
)

if(MSVC)
  target_compile_options(vbcc PRIVATE /W4 /WX /wd5030 /bigobj)
else()
  target_compile_options(vbcc PRIVATE
    -Wall -Wextra -Werror -Wshadow -Wno-zero-length-array)
endif()

target_link_libraries(vc
  vbc::vbcc
  CLI11::CLI11
)

install(TARGETS vc RUNTIME DESTINATION vc)
