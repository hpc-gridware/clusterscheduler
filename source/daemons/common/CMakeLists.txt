# source/daemons/common
set(LIBRARY_NAME daemonscommon)
set(LIBRARY_SOURCES
    "admin_mail.c"
    "category.c"
    "err_trace.c"
    "lock.c"
    "mail.c"
    "pdc.c"
    "procfs.c"
    "qlogin_starter.c"
    "qmaster_heartbeat.c"
    "setosjobid.c"
    "sge_processes_irix.c"
    "shutdown.c"
    "startprog.c"
    "unparse_job_cull.c")
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
                                                  ${LIBRARY_INCLUDES})
