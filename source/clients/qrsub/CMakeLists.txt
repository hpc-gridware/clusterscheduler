# source/clients/qrsub
add_executable(
  qrsub
  qrsub.cc
  ../../common/parse_job_cull.cc
  ../../common/parse_qsub.cc
  ../../common/sig_handlers.cc
  ../../common/sge_options.cc
  ../../common/sge_mt_init.cc
  ../../common/usage.cc)

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
          ${SGE_LIBS})

if(INSTALL_SGE_BIN)
  install(TARGETS qrsub DESTINATION bin/${SGE_ARCH})
endif()
