add_executable(vc
  passes/anf.cc
  passes/application.cc
  passes/ident.cc
  passes/operators.cc
  passes/parser.cc
  passes/reify.cc
  passes/sugar.cc
  passes/structure.cc
  lang.cc
  main.cc
  pathreification.cc
  reification.cc
  reifications.cc
  subtype.cc
)

if(MSVC)
  target_compile_options(vbcc PRIVATE /W4 /WX /wd5030 /bigobj)
else()
  target_compile_options(vbcc PRIVATE
    -Wall -Wextra -Werror -Wno-zero-length-array)
endif()

target_link_libraries(vc
  vbc::vbcc
  OpenSSL::Crypto
  OpenSSL::SSL
  CLI11::CLI11
  libgit2
  libgit2package
)

# Copy the stdlib to the install directory
install(DIRECTORY builtin DESTINATION vc)

install(TARGETS vc RUNTIME DESTINATION vc)
