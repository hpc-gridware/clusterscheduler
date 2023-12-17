# source/clients/qstat
add_executable(
  qstat
  qstat.c
  qstat_cmdline.c
  qstat_xml.c
  ../../common/parse_job_cull.c
  ../../common/parse_qsub.c
  ../../common/sig_handlers.c
  ../../common/sge_options.c
  ../../common/sge_mt_init.c
  ../../common/usage.c)

target_include_directories(qstat PRIVATE ${SGE_INCLUDES} "../../libs/uti"
                                         "../../libs/cull" "../../libs/sgeobj")

target_link_libraries(
  qstat
  PRIVATE clientscommon
          sched
          gdi
          sgeobj
          cull
          comm
          commlists
          uti
          ${SGE_TOPO_LIB}
          dl
          m)

install(TARGETS qstat DESTINATION bin/${SGE_ARCH})

install(
  CODE "execute_process(
    COMMAND /bin/sh -c \"
    cd ${CMAKE_INSTALL_PREFIX}/bin/${SGE_ARCH}
    ln -s qstat qselect
    \"
)")
