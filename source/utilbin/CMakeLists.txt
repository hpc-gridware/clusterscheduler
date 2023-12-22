# source/utilbin

# adminrun
add_executable(adminrun adminrun.cc)
target_include_directories(adminrun PRIVATE ${SGE_INCLUDES})
target_link_libraries(adminrun PRIVATE uti commlists)

# checkprog
add_executable(checkprog checkprog.cc)
target_include_directories(checkprog PRIVATE ${SGE_INCLUDES})
target_link_libraries(checkprog PRIVATE uti commlists dl)

# checkuser
add_executable(checkuser checkuser.cc)
target_include_directories(checkuser PRIVATE ${SGE_INCLUDES})
target_link_libraries(checkuser PRIVATE uti commlists dl)

# echo_raw
add_executable(echo_raw echo_raw.cc)
target_include_directories(echo_raw PRIVATE ${SGE_INCLUDES})
target_link_libraries(echo_raw PRIVATE uti commlists dl)

# filestat
add_executable(filestat filestat.cc)
target_include_directories(filestat PRIVATE ${SGE_INCLUDES})
target_link_libraries(filestat PRIVATE uti commlists dl)

# fstype
add_executable(fstype fstype.cc)
target_include_directories(fstype PRIVATE ${SGE_INCLUDES})
target_link_libraries(fstype PRIVATE uti commlists dl)

# gethostbyaddr
add_executable(gethostbyaddr gethostbyaddr.cc)
target_include_directories(gethostbyaddr PRIVATE ${SGE_INCLUDES})
target_link_libraries(gethostbyaddr PRIVATE gdi comm uti commlists dl)

# gethostbyname
add_executable(gethostbyname gethostbyname.cc)
target_include_directories(gethostbyname PRIVATE ${SGE_INCLUDES})
target_link_libraries(gethostbyname PRIVATE gdi comm commlists uti dl)

# gethostname
add_executable(gethostname gethostname.cc)
target_include_directories(gethostname PRIVATE ${SGE_INCLUDES})
target_link_libraries(gethostname PRIVATE gdi comm commlists uti dl)

# getservbyname
add_executable(getservbyname getservbyname.cc)
target_include_directories(getservbyname PRIVATE ${SGE_INCLUDES})
target_link_libraries(getservbyname PRIVATE gdi comm uti commlists dl)

# infotext
add_executable(infotext infotext.cc)
target_include_directories(infotext PRIVATE ${SGE_INCLUDES})
target_link_libraries(infotext PRIVATE gdi uti commlists dl)

# loadcheck
add_executable(loadcheck loadcheck.cc)
target_include_directories(loadcheck PRIVATE ${SGE_INCLUDES})
target_link_libraries(loadcheck PRIVATE sgeobj cull uti commlists
                                        ${SGE_TOPO_LIB} dl)

# now
add_executable(now now.cc)
target_include_directories(now PRIVATE ${SGE_INCLUDES})

# qping
add_executable(qping qping.cc)
target_include_directories(qping PRIVATE $SGE_INCLUDES)
target_link_libraries(
  qping
  PRIVATE gdi
          sgeobj
          cull
          comm
          uti
          commlists
          dl
          m)

# read_raw
add_executable(read_raw read_raw.cc)
target_include_directories(read_raw PRIVATE ${SGE_INCLUDES})

# sge_share_mon
add_executable(sge_share_mon sge_share_mon.cc)
target_include_directories(sge_share_mon PRIVATE ${SGE_INCLUDES})
target_link_libraries(
  sge_share_mon
  PRIVATE sched
          gdi
          sgeobj
          cull
          comm
          uti
          commlists
          dl
          m)

# spooldefaults
add_executable(spooldefaults spooldefaults.cc)
target_include_directories(spooldefaults PRIVATE ${SGE_INCLUDES})
target_link_libraries(
  spooldefaults
  PRIVATE
          spoolc
          ${SPOOLING_LIBS}
          sched
          gdi
          sgeobj
          cull
          comm
          uti
          commlists
          dl)

# spooledit
add_executable(spooledit spooledit.cc)
target_include_directories(spooledit PRIVATE ${SGE_INCLUDES})
target_link_libraries(
  spooledit
  PRIVATE
          #spoolc
          ${SPOOLING_LIBS}
          sched
          gdi
          sgeobj
          cull
          comm
          uti
          commlists
          dl)

# spoolinit
add_executable(spoolinit spoolinit.cc ../common/sge_mt_init.cc)
target_include_directories(spoolinit PRIVATE ${SGE_INCLUDES})
target_link_libraries(
  spoolinit
  PRIVATE
          #spoolc
          ${SPOOLING_LIBS}
          sched
          gdi
          sgeobj
          cull
          comm
          uti
          commlists
          dl)

# testsuidroot
add_executable(testsuidroot testsuidroot.cc)
target_include_directories(testsuidroot PRIVATE ${SGE_INCLUDES})
target_link_libraries(testsuidroot PRIVATE uti commlists dl)

# uidgid
add_executable(uidgid uidgid.cc)
target_include_directories(uidgid PRIVATE ${SGE_INCLUDES})
target_link_libraries(uidgid PRIVATE uti commlists dl)

# work
add_executable(work work.cc)
target_include_directories(work PRIVATE ${SGE_INCLUDES})
target_link_libraries(
  work
  PRIVATE gdi
          sgeobj
          cull
          comm
          uti
          commlists
          dl
          m)

# valid_jvmlib @todo remove

if(INSTALL_SGE_BIN)
  install(TARGETS qping DESTINATION bin/${SGE_ARCH})
  install(
    TARGETS adminrun
            checkprog
            checkuser
            echo_raw
            filestat
            fstype
            gethostbyaddr
            gethostbyname
            gethostname
            getservbyname
            infotext
            loadcheck
            now
            qping
            read_raw
            sge_share_mon
            spooldefaults
            spooledit
            spoolinit
            testsuidroot
            uidgid
    DESTINATION utilbin/${SGE_ARCH})
  install(TARGETS work DESTINATION examples/jobsbin/${SGE_ARCH})
endif()
