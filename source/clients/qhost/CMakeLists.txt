# source/clients/qhost
add_executable(
  qhost qhost.cc ../../common/sig_handlers.cc ../../common/sge_options.cc
        ../../common/sge_mt_init.cc ../../common/usage.cc)
target_include_directories(qhost PRIVATE ${SGE_INCLUDES})

target_link_libraries(
  qhost
  PRIVATE clientscommon
          comm
          commlists
          gdi
          sched
          sgeobj
          cull
          uti
          ${SGE_TOPO_LIB}
          ${SGE_LIBS})

if(INSTALL_SGE_BIN)
  install(TARGETS qhost DESTINATION bin/${SGE_ARCH})
endif()
