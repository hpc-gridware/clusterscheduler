# source/daemons/shepherd
add_executable(sge_shepherd
  am_chdir.cc
  builtin_starter.cc
  setjoblimit.cc
  setrlimits.cc
  sge_fileio.cc
  sge_pset.cc
  sge_shepconf.cc
  sge_shepherd_ijs.cc
  shepherd.cc
  shepherd_binding.cc
  signal_queue.cc
  ../../common/sge_mt_init.cc
  ../../common/execution_states.cc
  ../../common/sge_ijs_comm.cc
  ../../common/sge_ijs_threads.cc)

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

if(INSTALL_SGE_BIN)
  install(TARGETS sge_shepherd DESTINATION bin/${SGE_ARCH})
endif()
