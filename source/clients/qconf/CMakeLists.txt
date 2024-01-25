# source/clients/qconf
add_executable(
      qconf
      oge_qconf_cqueue.cc
      oge_qconf_centry.cc
      sge_qconf_hgroup.cc
      oge_qconf_rqs.cc
      oge_qconf.cc
      oge_qconf_parse.cc
      oge_qconf_acl.cc
      ../../common/sig_handlers.cc
      ../../common/sge_options.cc
      ../../common/sge_mt_init.cc
      ../../common/usage.cc)
target_include_directories(qconf PUBLIC ${SGE_INCLUDES})
target_link_libraries(
      qconf
      PUBLIC clientscommon
      spoolc_static
      spool
      sched
      comm
      commlists
      gdi
      sgeobj
      cull
      uti
      ${SGE_LIBS})

if (INSTALL_SGE_BIN)
   install(TARGETS qconf DESTINATION bin/${SGE_ARCH})
endif ()
