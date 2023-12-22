# source/clients/qrstat
add_executable(
  qrstat
  qrstat.cc
  qrstat_filter.cc
  qrstat_report_handler.cc
  qrstat_report_handler_stdout.cc
  qrstat_report_handler_xml.cc
  ../../common/parse_job_cull.cc
  ../../common/parse_qsub.cc
  ../../common/sig_handlers.cc
  ../../common/sge_options.cc
  ../../common/usage.cc)

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
