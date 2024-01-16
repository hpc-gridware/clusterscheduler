# source/clients/qalter
add_executable(
  qalter
        oge_qalter.cc
  ../../common/parse_job_cull.cc
  ../../common/parse_qsub.cc
  ../../common/sig_handlers.cc
  ../../common/sge_options.cc
  ../../common/sge_mt_init.cc
  ../../common/usage.cc)
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
          ${SGE_LIBS})

if(INSTALL_SGE_BIN)
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
endif()
