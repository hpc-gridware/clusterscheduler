# source/daemons/common
set(LIBRARY_NAME daemonscommon)
set(LIBRARY_SOURCES
      admin_mail.cc
      category.cc
      err_trace.cc
      lock.cc
      mail.cc
      pdc.cc
      procfs.cc
      qlogin_starter.cc
      qmaster_heartbeat.cc
      setosjobid.cc
      shutdown.cc
      startprog.cc
      unparse_job_cull.cc)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
      ${LIBRARY_INCLUDES})

