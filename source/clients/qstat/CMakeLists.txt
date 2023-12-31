# source/clients/qstat
add_executable(
  qstat
  qstat.cc
  qstat_cmdline.cc
  qstat_xml.cc
  ../../common/parse_job_cull.cc
  ../../common/parse_qsub.cc
  ../../common/sig_handlers.cc
  ../../common/sge_options.cc
  ../../common/sge_mt_init.cc
  ../../common/usage.cc)

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
          ${SGE_LIBS})

if(INSTALL_SGE_BIN)
  install(TARGETS qstat DESTINATION bin/${SGE_ARCH})

  install(
    CODE "execute_process(
    COMMAND /bin/sh -c \"
    cd ${CMAKE_INSTALL_PREFIX}/bin/${SGE_ARCH}
    ln -s qstat qselect
    \"
)")
endif()
