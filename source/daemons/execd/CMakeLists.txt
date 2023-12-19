# source/daemons/execd
add_executable(
  sge_execd
  execd.c
  dispatcher.c
  exec_job.c
  execd_ck_to_do.c
  execd_get_new_conf.c
  execd_job_exec.c
  execd_kill_execd.c
  execd_signal_queue.c
  execd_ticket.c
  get_path.c
  job_report_execd.c
  load_avg.c
  ptf.c
  reaper_execd.c
  setup_execd.c
  sge_load_sensor.c
  sge_report_execd.c
  tmpdir.c
  ../../common/sig_handlers.c
  ../../common/sge_options.c
  ../../common/sge_mt_init.c
  ../../common/usage.c
  ../../common/execution_states.c)

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
          dl)

if(INSTALL_SGE_BIN)
  install(TARGETS sge_execd DESTINATION bin/${SGE_ARCH})
endif()
