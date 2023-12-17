# source/clients/qalter
add_executable(
  qalter
  qalter.c
  ../../common/parse_job_cull.c
  ../../common/parse_qsub.c
  ../../common/sig_handlers.c
  ../../common/sge_options.c
  ../../common/sge_mt_init.c
  ../../common/usage.c)
target_include_directories(qalter PRIVATE ${SGE_INCLUDES})

target_link_libraries(
  qalter
  PRIVATE clientscommon
          gdi
          sgeobj
          cull
          comm
          commlists
          uti
          ${SGE_TOPO_LIB}
          dl)

install(TARGETS qalter DESTINATION bin/${SGE_ARCH})

install(
  CODE "execute_process(
    COMMAND /bin/sh -c \"
    cd ${CMAKE_INSTALL_PREFIX}/bin/${SGE_ARCH}
    ln -s qalter qhold
    ln -s qalter qresub
    ln -s qalter qrls
    \"
)")
