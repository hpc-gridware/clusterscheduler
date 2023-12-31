# source/clients/qdel
add_executable(qdel qdel.cc ../../common/sge_options.cc ../../common/usage.cc)
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
         ${SGE_LIBS})

if(INSTALL_SGE_BIN)
  install(TARGETS qdel DESTINATION bin/${SGE_ARCH})
endif()
