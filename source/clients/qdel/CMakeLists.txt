# source/clients/qdel
add_executable(qdel qdel.c ../../common/sge_options.c ../../common/usage.c)
target_include_directories(qdel PUBLIC ${SGE_INCLUDES})
target_link_libraries(
  qdel
  PUBLIC clientscommon
         comm
         commlists
         gdi
         sgeobj
         cull
         uti
         dl)

if(INSTALL_SGE_BIN)
  install(TARGETS qdel DESTINATION bin/${SGE_ARCH})
endif()
