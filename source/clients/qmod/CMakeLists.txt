# source/clients/qmod
add_executable(qmod qmod.c ../../common/sig_handlers.c
                    ../../common/sge_options.c ../../common/usage.c)
target_include_directories(qmod PRIVATE ${SGE_INCLUDES})
target_link_libraries(
  qmod
  PRIVATE gdi
          sgeobj
          cull
          comm
          commlists
          uti
          dl)

if(INSTALL_SGE_BIN)
  install(TARGETS qmod DESTINATION bin/${SGE_ARCH})
endif()
