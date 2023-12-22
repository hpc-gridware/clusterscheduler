# source/clients/qsh
add_executable(
  qsh
  qsh.cc
  parse_job_qsh.cc
  ../../common/parse_job_cull.cc
  ../../common/parse_qsub.cc
  ../../common/sig_handlers.cc
  ../../common/sge_options.cc
  # ../../common/sge_mt_init.cc
  ../../common/usage.cc
  ../../common/sge_ijs_comm.cc
  ../../common/sge_ijs_threads.cc)
target_include_directories(qsh PRIVATE ${SGE_INCLUDES})

target_link_libraries(
  qsh
  PRIVATE clientscommon
          gdi
          sgeobj
          cull
          comm
          commlists
          uti
          ijs
          ${SGE_TOPO_LIB}
          dl)

if(INSTALL_SGE_BIN)
  install(TARGETS qsh DESTINATION bin/${SGE_ARCH})

  install(
    CODE "execute_process(
    COMMAND /bin/sh -c \"
    cd ${CMAKE_INSTALL_PREFIX}/bin/${SGE_ARCH}
    ln -s qsh qlogin
    ln -s qsh qrsh
    \"
)")
endif()
