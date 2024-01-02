# source/clients/qconf
add_executable(
  qconf
  qconf.cc
  parse_qconf.cc
  sge_client_access.cc
  ../../common/sig_handlers.cc
  ../../common/sge_options.cc
  ../../common/sge_mt_init.cc
  ../../common/usage.cc)
target_include_directories(qconf PUBLIC ${SGE_INCLUDES})
target_link_libraries(
  qconf
  PUBLIC clientscommon
         spoolc_static
         spool
         sched
         comm
         commlists
         gdi
         sgeobj
         cull
         uti
         ${SGE_LIBS})

if(INSTALL_SGE_BIN)
  install(TARGETS qconf DESTINATION bin/${SGE_ARCH})
endif()
