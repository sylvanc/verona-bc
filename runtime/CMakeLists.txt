add_library(libvbcrt STATIC
  exit_code.cc
  native_main.cc
)

target_link_libraries(libvbcrt PUBLIC
  vbc::include
)

add_library(vbc::vbcrt ALIAS libvbcrt)

set_target_properties(libvbcrt PROPERTIES OUTPUT_NAME vbcrt)

if(MSVC)
  target_compile_options(libvbcrt PRIVATE /W4 /WX)
else()
  target_compile_options(libvbcrt PRIVATE -Wall -Wextra -Werror)
endif()

install(TARGETS libvbcrt ARCHIVE DESTINATION lib)
install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/include/vbcrt
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
