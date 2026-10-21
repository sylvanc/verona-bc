add_library(virc_vbc STATIC
  debug_info.cc
  emitter.cc
  encoder.cc
  instruction_encoder.cc
  string_table.cc
  type_encoding.cc
)

target_link_libraries(virc_vbc PUBLIC
  virc_core
  vbc::include
  deps::zstd
)

install(TARGETS virc_vbc ARCHIVE DESTINATION lib/virc)