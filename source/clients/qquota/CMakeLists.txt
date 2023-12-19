# source/clients/qquota
add_executable(
  qquota
  qquota.c
  ../../common/parse_job_cull.c
  ../../common/parse_qsub.c
  ../../common/sig_handlers.c
  ../../common/sge_mt_init.c
  ../../common/sge_options.c
  ../../common/usage.c)
target_include_directories(qquota PRIVATE ${SGE_INCLUDES})

target_link_libraries(
  qquota
  PRIVATE clientscommon
          sched
          gdi
          sgeobj
          cull
          comm
          commlists
          uti
          ${SGE_TOPO_LIB}
          dl
          m)

if(INSTALL_SGE_BIN)
  install(TARGETS qquota DESTINATION bin/${SGE_ARCH})
endif()
