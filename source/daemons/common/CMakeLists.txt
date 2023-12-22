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
    sge_processes_irix.cc
    shutdown.cc
    startprog.cc
    unparse_job_cull.cc)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
                                                  ${LIBRARY_INCLUDES})

add_executable(test_category test_category.cc)
target_include_directories(test_category PRIVATE "./")
target_link_libraries(test_category PRIVATE daemonscommon sched sgeobj cull comm uti commlists)

add_executable(test_heartbeat test_heartbeat.cc)
target_include_directories(test_heartbeat PRIVATE "./")
target_link_libraries(test_heartbeat PRIVATE daemonscommon uti commlists)

if(INSTALL_SGE_TEST)
    install(TARGETS test_category DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_heartbeat DESTINATION testbin/${SGE_ARCH})
endif()
