# source/daemons/shepherd
add_executable(
  sge_shepherd
  am_chdir.c
  builtin_starter.c
  setjoblimit.c
  setrlimits.c
  sge_fileio.c
  sge_pset.c
  sge_shepconf.c
  sge_shepherd_ijs.c
  shepherd.c
  shepherd_binding.c
  signal_queue.c
  ../../common/sge_mt_init.c
  ../../common/execution_states.c
  ../../common/sge_ijs_comm.c
  ../../common/sge_ijs_threads.c)

target_include_directories(sge_shepherd PRIVATE ${SGE_INCLUDES})

target_link_libraries(
  sge_shepherd
  PRIVATE daemonscommon
          gdi
          sgeobj
          cull
          comm
          commlists
          uti
          ${SGE_TOPO_LIB}
          dl)

install(TARGETS sge_shepherd DESTINATION bin/${SGE_ARCH})
