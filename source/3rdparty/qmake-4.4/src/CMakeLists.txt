# source/3rdparty/qmake-4.4/src

add_custom_command(
  OUTPUT config.h COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/config.h.${SGE_ARCH}
                          config.h)
add_executable(
  qmake
  ar.c
  arscan.c
  commands.c
  config.h
  default.c
  dir.c
  expand.c
  file.c
  function.c
  getopt1.c
  getopt.c
  guile.c
  hash.c
  implicit.c
  job.c
  loadapi.c
  load.c
  main.c
  misc.c
  output.c
  posixos.c
  read.c
  remake.c
  remote-sge.c
  rule.c
  shuffle.c
  signame.c
  strcache.c
  variable.c
  version.c
  vpath.c)
target_compile_definitions(
  qmake
  PRIVATE LOCALEDIR="/usr/local/share/locale" INCLUDEDIR="/usr/local/include"
          LIBDIR="/usr/local/lib" DHAVE_CONFIG_H)
target_compile_options(qmake PRIVATE -Wno-error)
target_include_directories(
  qmake
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_LIST_DIR}
          ${CMAKE_CURRENT_LIST_DIR}/../lib
)
target_link_libraries(qmake PRIVATE dl)

if(INSTALL_SGE_BIN)
  install(TARGETS qmake DESTINATION bin/${SGE_ARCH})
endif()
