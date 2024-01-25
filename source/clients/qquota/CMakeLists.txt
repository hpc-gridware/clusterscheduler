# source/clients/qquota
add_executable(
      qquota
      oge_qquota.cc
      oge_qquota_print.cc
      ../../common/parse_job_cull.cc
      ../../common/parse_qsub.cc
      ../../common/sig_handlers.cc
      ../../common/sge_mt_init.cc
      ../../common/sge_options.cc
      ../../common/usage.cc)
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
      ${SGE_LIBS})

if (INSTALL_SGE_BIN)
   install(TARGETS qquota DESTINATION bin/${SGE_ARCH})
endif ()
