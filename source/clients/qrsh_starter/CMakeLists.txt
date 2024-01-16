# source/clients/qrsh
add_executable(qrsh_starter oge_qrsh_starter.cc)
target_include_directories(qrsh_starter PRIVATE ${SGE_INCLUDES})
target_link_libraries(qrsh_starter PRIVATE uti commlists)

if(INSTALL_SGE_BIN)
  install(TARGETS qrsh_starter DESTINATION utilbin/${SGE_ARCH})
endif()
