# source/clients/qrstat
add_executable(
  qrstat
  qrstat.c
  qrstat_filter.c
  qrstat_report_handler.c
  qrstat_report_handler_stdout.c
  qrstat_report_handler_xml.c
  ../../common/parse_job_cull.c
  ../../common/parse_qsub.c
  ../../common/sig_handlers.c
  ../../common/sge_options.c
  ../../common/usage.c)

target_include_directories(qrstat PRIVATE ${SGE_INCLUDES})

target_link_libraries(
  qrstat
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
  install(TARGETS qrstat DESTINATION bin/${SGE_ARCH})
endif()
