# source/clients/showq
add_executable(
  showq
  showq.c showq_cmdline_tacc.c showq_support.c ../../common/parse_job_cull.c
  ../../common/parse_qsub.c ../../common/sig_handlers.c
  # ../../common/sge_options.c
  # ../../common/sge_mt_init.c
  # ../../common/usage.c
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
         dl
         m)

if(INSTALL_SGE_BIN)
  install(TARGETS showq DESTINATION examples/jobsbin/${SGE_ARCH})
endif()
