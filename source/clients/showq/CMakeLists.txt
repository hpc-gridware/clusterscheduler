# source/clients/showq
add_executable(
      showq
      oge_showq.cc oge_showq_cmdline_tacc.cc oge_showq_support.cc ../../common/parse_job_cull.cc
      ../../common/parse_qsub.cc ../../common/sig_handlers.cc
      # ../../common/sge_options.cc
      # ../../common/sge_mt_init.cc
      # ../../common/usage.cc
)
target_include_directories(showq PUBLIC ${SGE_INCLUDES})
target_link_libraries(
      showq
      PUBLIC sched
      # evc
      gdi
      sgeobj
      cull
      comm
      commlists
      uti
      ${SGE_TOPO_LIB}
      ${SGE_LIBS})

if (INSTALL_SGE_BIN)
   install(TARGETS showq DESTINATION examples/jobsbin/${SGE_ARCH})
endif ()
