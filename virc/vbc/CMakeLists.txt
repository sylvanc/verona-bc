add_library(virc_vbc STATIC
  emitter.cc
)

target_link_libraries(virc_vbc PUBLIC
  virc_core
  verona::include
  deps::zstd
)

install(TARGETS virc_vbc ARCHIVE DESTINATION lib/virc)
