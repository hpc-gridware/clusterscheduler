#___INFO__MARK_BEGIN_NEW__
#___INFO__MARK_END_NEW__

# source/3rdparty/qmake-4.4/src

if(SGE_ARCH MATCHES "darwin-arm64")
  add_custom_command(OUTPUT glob.h COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/glob.h.${SGE_ARCH} glob.h)
  set(ADD_QMAKE_SOURCES glob.h ../lib/concat-filename.c ../lib/findprog-in.c ../lib/fnmatch.c)
elseif(SGE_ARCH MATCHES "fbsd-amd64")
  add_custom_command(OUTPUT glob.h COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/glob.h.${SGE_ARCH} glob.h)
  set(ADD_QMAKE_SOURCES glob.h ../lib/concat-filename.c ../lib/findprog-in.c)
elseif(SGE_ARCH MATCHES "lx-riscv64")
  set(ADD_QMAKE_SOURCES ../lib/concat-filename.c ../lib/findprog-in.c)
elseif(SGE_ARCH MATCHES "lx-s390x")
  set(ADD_QMAKE_SOURCES ../lib/concat-filename.c ../lib/findprog-in.c)
elseif(SGE_ARCH MATCHES "lx-ppc64le")
  set(ADD_QMAKE_SOURCES ../lib/concat-filename.c ../lib/findprog-in.c)
elseif(SGE_ARCH MATCHES "sol-amd64")
  add_custom_command(OUTPUT glob.h COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/glob.h.${SGE_ARCH} glob.h)
  set(ADD_QMAKE_SOURCES glob.h ../lib/glob.c ../lib/concat-filename.c ../lib/findprog-in.c)
  add_compile_options(-Wno-builtin-declaration-mismatch)
  add_compile_definitions(HAVE_CONFIG_H)
elseif(SGE_ARCH MATCHES "osol-amd64")
  add_custom_command(OUTPUT glob.h COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/glob.h.${SGE_ARCH} glob.h)
  set(ADD_QMAKE_SOURCES glob.h ../lib/glob.c ../lib/concat-filename.c ../lib/findprog-in.c)
  add_compile_options(-Wno-builtin-declaration-mismatch)
  add_compile_definitions(HAVE_CONFIG_H)
  set(GEN_LIB gen)
else()
  set(ADD_QMAKE_SOURCES "")
endif()

add_custom_command(
  OUTPUT config.h COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/config.h.${SGE_ARCH} config.h)

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
  vpath.c
  ${ADD_QMAKE_SOURCES})
target_compile_definitions(
  qmake
  PRIVATE LOCALEDIR="/usr/local/share/locale" INCLUDEDIR="/usr/local/include"
          LIBDIR="/usr/local/lib" DHAVE_CONFIG_H)
target_compile_options(qmake PRIVATE -Wno-error)
target_include_directories(
  qmake
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_LIST_DIR}
          ${CMAKE_CURRENT_LIST_DIR}/../lib
          ${SGE_INCLUDES} /usr/local/include
)
target_link_libraries(qmake PRIVATE ${GEN_LIB} ${SGE_LIBS})

if(INSTALL_SGE_BIN)
  install(TARGETS qmake DESTINATION bin/${SGE_ARCH})
endif()
