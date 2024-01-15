# source/clients/qrstat
add_executable(
  qrstat
        oge_qrstat.cc
        oge_qrstat_filter.cc
        oge_qrstat_report_handler.cc
        oge_qrstat_report_handler_stdout.cc
        oge_qrstat_report_handler_xml.cc
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
          ${SGE_LIBS})

if(INSTALL_SGE_BIN)
  install(TARGETS qrstat DESTINATION bin/${SGE_ARCH})
endif()
