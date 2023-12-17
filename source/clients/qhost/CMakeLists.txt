# source/clients/qhost
add_executable(
  qhost qhost.c ../../common/sig_handlers.c ../../common/sge_options.c
        ../../common/sge_mt_init.c ../../common/usage.c)
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
          dl
          m)

install(TARGETS qhost DESTINATION bin/${SGE_ARCH})
