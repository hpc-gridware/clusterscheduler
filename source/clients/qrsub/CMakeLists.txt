# source/clients/qrsub
add_executable(
  qrsub
  qrsub.c
  ../../common/parse_job_cull.c
  ../../common/parse_qsub.c
  ../../common/sig_handlers.c
  ../../common/sge_options.c
  ../../common/sge_mt_init.c
  ../../common/usage.c)

target_include_directories(qrsub PRIVATE ${SGE_INCLUDES})

target_link_libraries(
  qrsub
  PRIVATE clientscommon
          gdi
          sgeobj
          cull
          comm
          commlists
          uti
          ${SGE_TOPO_LIB}
          dl)

if(INSTALL_SGE_BIN)
  install(TARGETS qrsub DESTINATION bin/${SGE_ARCH})
endif()
