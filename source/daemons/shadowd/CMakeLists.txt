# source/daemons/shadowd
add_executable(sge_shadowd
      shadowd.cc
      ../../common/sig_handlers.cc
      ../../common/sge_options.cc
      ../../common/usage.cc)

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
      ${SGE_LIBS})

if (INSTALL_SGE_BIN)
   install(TARGETS sge_shadowd DESTINATION bin/${SGE_ARCH})
endif ()
