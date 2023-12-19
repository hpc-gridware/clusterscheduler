# source/clients/qsub
add_executable(
  qsub
  qsub.c
  ../../common/parse_job_cull.c
  ../../common/parse_qsub.c
  ../../common/sig_handlers.c
  ../../common/sge_options.c
  ../../common/sge_mt_init.c
  ../../common/usage.c
  ../../libs/japi/japi.c)
target_include_directories(qsub PRIVATE ${SGE_INCLUDES})

target_link_libraries(
  qsub
  PRIVATE clientscommon
          comm
          commlists
          evc
          gdi
          sgeobj
          cull
          uti
          ${SGE_TOPO_LIB}
          dl)

if(INSTALL_SGE_BIN)
  install(TARGETS qsub DESTINATION bin/${SGE_ARCH})
endif()
