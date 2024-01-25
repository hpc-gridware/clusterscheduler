# source/clients/qevent
add_executable(
      qevent oge_qevent.cc ../../common/sig_handlers.cc ../../common/sge_options.cc
      ../../common/sge_mt_init.cc ../../common/usage.cc)
target_include_directories(qevent PUBLIC ${SGE_INCLUDES})
target_link_libraries(
      qevent
      PUBLIC clientscommon
      comm
      commlists
      mir
      evc
      gdi
      sgeobj
      cull
      uti
      ${SGE_LIBS})

if (INSTALL_SGE_TEST)
   install(TARGETS qevent DESTINATION testbin/${SGE_ARCH})
endif ()
