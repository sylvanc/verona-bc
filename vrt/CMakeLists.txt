add_library(libvrt STATIC
  exit_code.cc
  frame.cc
  native_main.cc
  thread.cc
)

target_link_libraries(libvrt PUBLIC
  vbc::include
)

add_library(vbc::vrt ALIAS libvrt)

set_target_properties(libvrt PROPERTIES OUTPUT_NAME vrt)

if(MSVC)
  target_compile_options(libvrt PRIVATE /W4 /WX)
else()
  target_compile_options(libvrt PRIVATE -Wall -Wextra -Werror)
endif()

install(TARGETS libvrt ARCHIVE DESTINATION lib)
install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/include/vrt
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
