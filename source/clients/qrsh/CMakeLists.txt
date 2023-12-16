# source/clients/qrsh
add_executable(qrsh_starter qrsh_starter.c)
target_include_directories(qrsh_starter PRIVATE ${SGE_INCLUDES})
target_link_libraries(qrsh_starter PRIVATE uti commlists)

install(TARGETS qrsh_starter DESTINATION utilbin/${SGE_ARCH})
