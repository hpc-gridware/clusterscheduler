# source/daemons/execd
add_executable(
  sge_execd
  execd.cc
  dispatcher.cc
  exec_job.cc
  execd_ck_to_do.cc
  execd_get_new_conf.cc
  execd_job_exec.cc
  execd_kill_execd.cc
  execd_signal_queue.cc
  execd_ticket.cc
  get_path.cc
  job_report_execd.cc
  load_avg.cc
  ptf.cc
  reaper_execd.cc
  setup_execd.cc
  sge_load_sensor.cc
  sge_report_execd.cc
  tmpdir.cc
  ../../common/sig_handlers.cc
  ../../common/sge_options.cc
  ../../common/sge_mt_init.cc
  ../../common/usage.cc
  ../../common/execution_states.cc)

target_include_directories(sge_execd PRIVATE ${SGE_INCLUDES})

target_link_libraries(
  sge_execd
  PRIVATE daemonscommon
          spoolc
          spool
          sched
          gdi
          sgeobj
          cull
          comm
          commlists
          uti
          ${SGE_TOPO_LIB}
          ${SGE_LIBS})

if(INSTALL_SGE_BIN)
  install(TARGETS sge_execd DESTINATION bin/${SGE_ARCH})
endif()
