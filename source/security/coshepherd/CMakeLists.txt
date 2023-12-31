# security/coshepherd

add_executable(sge_coshepherd coshepherd.c)

target_include_directories(sge_coshepherd PRIVATE ${SGE_INCLUDES})

target_link_libraries(sge_coshepherd PRIVATE gdi uti commlists ${SGE_LIBS})

if(INSTALL_SGE_BIN)
  install(TARGETS sge_coshepherd DESTINATION bin/${SGE_ARCH})
endif()
