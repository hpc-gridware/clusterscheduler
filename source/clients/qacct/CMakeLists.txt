# source/clients/qacct
add_executable(qacct qacct.c ../../common/execution_states.c
                     ../../common/sig_handlers.c)
target_include_directories(qacct PRIVATE ${SGE_INCLUDES})
target_link_libraries(
  qacct
  PRIVATE comm
          commlists
          cull
          gdi
          sched
          sgeobj
          uti
          ${SGE_LIBS})

install(TARGETS qacct DESTINATION bin/${SGE_ARCH})
