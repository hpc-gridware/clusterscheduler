# source/clients/qrdel
add_executable(
  qrdel
  qrdel.c ../../common/parse_job_cull.c ../../common/parse_qsub.c
  ../../common/sig_handlers.c ../../common/sge_options.c ../../common/usage.c)
target_include_directories(qrdel PRIVATE ${SGE_INCLUDES})

target_link_libraries(
  qrdel
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
  install(TARGETS qrdel DESTINATION bin/${SGE_ARCH})
endif()
