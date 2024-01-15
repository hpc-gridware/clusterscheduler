# source/dist

if(INSTALL_SGE_COMMON)
  install(PROGRAMS install_execd install_qmaster inst_sge DESTINATION .)

  install(
    DIRECTORY 3rd_party
              ckpt
              examples
              hadoop
              mpi
              util
    DESTINATION .
    USE_SOURCE_PERMISSIONS)
endif()
