# source/daemons/shadowd
add_executable(
  sge_shadowd shadowd.c ../../common/sig_handlers.c ../../common/sge_options.c
              ../../common/sge_mt_init.c ../../common/usage.c)
target_include_directories(sge_shadowd PRIVATE ${SGE_INCLUDES})
target_link_libraries(
  sge_shadowd
  PRIVATE daemonscommon
          gdi
          sgeobj
          cull
          comm
          commlists
          uti
          dl)

if(INSTALL_SGE_BIN)
  install(TARGETS sge_shadowd DESTINATION bin/${SGE_ARCH})
endif()
