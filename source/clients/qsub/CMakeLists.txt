# source/clients/qsub
add_executable(
  qsub
  qsub.cc
  ../../common/parse_job_cull.cc
  ../../common/parse_qsub.cc
  ../../common/sig_handlers.cc
  ../../common/sge_options.cc
  ../../common/sge_mt_init.cc
  ../../common/usage.cc
  ../../libs/japi/japi.cc)
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
