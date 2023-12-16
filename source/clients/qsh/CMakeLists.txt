# source/clients/qsh
add_executable(
  qsh
  qsh.c
  parse_job_qsh.c
  ../../common/parse_job_cull.c
  ../../common/parse_qsub.c
  ../../common/sig_handlers.c
  ../../common/sge_options.c
  # ../../common/sge_mt_init.c
  ../../common/usage.c
  ../../common/sge_ijs_comm.c
  ../../common/sge_ijs_threads.c)
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
          plpa
          dl)

install(TARGETS qsh DESTINATION bin/${SGE_ARCH})

install(
  CODE "execute_process(
    COMMAND /bin/sh -c \"
    cd ${CMAKE_INSTALL_PREFIX}/bin/${SGE_ARCH}
    ln -s qsh qlogin
    ln -s qsh qrsh
    \"
)")
