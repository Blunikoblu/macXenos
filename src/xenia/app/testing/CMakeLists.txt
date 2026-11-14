xe_test_suite(xenia-app-tests ${CMAKE_CURRENT_SOURCE_DIR}
  LINKS fmt xenia-base xenia-vfs
)
target_sources(xenia-app-tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../game_library.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/../storage_migration_mac.cc
)
