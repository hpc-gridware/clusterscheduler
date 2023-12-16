# source/clients/qevent
add_executable(
  qevent qevent.c ../../common/sig_handlers.c ../../common/sge_options.c
         ../../common/sge_mt_init.c ../../common/usage.c)
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
         dl)

install(TARGETS qevent DESTINATION test/bin/${SGE_ARCH})
