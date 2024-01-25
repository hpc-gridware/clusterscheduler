# source/clients/qrdel
add_executable(
      qrdel
      oge_qrdel.cc ../../common/parse_job_cull.cc ../../common/parse_qsub.cc
      ../../common/sig_handlers.cc ../../common/sge_options.cc ../../common/usage.cc)
target_include_directories(qrdel PRIVATE ${SGE_INCLUDES})

target_link_libraries(
      qrdel
      PRIVATE clientscommon
      gdi
      sgeobj
      cull
      comm
      commlists
      uti
      ${SGE_TOPO_LIB}
      ${SGE_LIBS})

if (INSTALL_SGE_BIN)
   install(TARGETS qrdel DESTINATION bin/${SGE_ARCH})
endif ()
