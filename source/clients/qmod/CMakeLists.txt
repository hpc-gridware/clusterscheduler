# source/clients/qmod
add_executable(qmod oge_qmod.cc ../../common/sig_handlers.cc
      ../../common/sge_options.cc ../../common/usage.cc)
target_include_directories(qmod PRIVATE ${SGE_INCLUDES})
target_link_libraries(
      qmod
      PRIVATE gdi
      sgeobj
      cull
      comm
      commlists
      uti
      ${SGE_LIBS})

if (INSTALL_SGE_BIN)
   install(TARGETS qmod DESTINATION bin/${SGE_ARCH})
endif ()
