# source/clients/qconf
add_executable(
  qconf
  qconf.c
  parse_qconf.c
  sge_client_access.c
  ../../common/sig_handlers.c
  ../../common/sge_options.c
  ../../common/sge_mt_init.c
  ../../common/usage.c)
target_include_directories(qconf PUBLIC ${SGE_INCLUDES})
target_link_libraries(
  qconf
  PUBLIC clientscommon
         spoolc
         spool
         sched
         comm
         commlists
         gdi
         sgeobj
         cull
         uti
         dl)

install(TARGETS qconf DESTINATION bin/${SGE_ARCH})
