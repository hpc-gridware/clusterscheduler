# source/libs/uti
set(LIBRARY_NAME uti)
set(LIBRARY_SOURCES
    config_file.c
    setup_path.c
    sge_afsutil.c
    sge_arch.c
    sge_base64.c
    sge_binding_hlp.c
    sge_binding_parse.c
    sge_bitfield.c
    sge_bootstrap.c
    sge_csp_path.c
    sge_dstring.c
    sge_edit.c
    sge_env.c
    sge_err.c
    sge_error_class.c
    sge_fgl.c
    sge_getloadavg.c
    sge_hostname.c
    sge_htable.c
    sge_io.c
    sge_language.c
    sge_loadmem.c
    sge_lock.c
    sge_lock_fifo.c
    sge_log.c
    sge_monitor.c
    sge_mtutil.c
    sge_nprocs.c
    sge_os.c
    sge_parse_args.c
    sge_parse_num_par.c
    sge_profiling.c
    sge_prog.c
    sge_pty.c
    sge_rmon_macros.c
    sge_rmon_monitoring_level.c
    sge_signal.c
    sge_sl.c
    sge_smf.c
    sge_spool.c
    sge_stdio.c
    sge_stdlib.c
    sge_string.c
    sge_thread_ctrl.c
    sge_time.c
    sge_tmpnam.c
    sge_tq.c
    sge_uidgid.c
    sge_unistd.c)
set(LIBRARY_INCLUDES "./" ${SGE_INCLUDES})

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${LIBRARY_INCLUDES})

add_executable(test_sge_bitfield test_sge_bitfield.c)
target_include_directories(test_sge_bitfield PRIVATE "./")
target_link_libraries(test_sge_bitfield PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_deadlock test_sge_lock_main.c test_sge_deadlock.c)
target_include_directories(test_sge_deadlock PRIVATE "./")
target_link_libraries(test_sge_deadlock PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_dstring test_sge_dstring.c)
target_include_directories(test_sge_dstring PRIVATE "./")
target_link_libraries(test_sge_dstring PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_err test_sge_err.c)
target_include_directories(test_sge_err PRIVATE "./")
target_link_libraries(test_sge_err PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_lock_simple test_sge_lock_main.c test_sge_lock_simple.c)
target_include_directories(test_sge_lock_simple PRIVATE "./")
target_link_libraries(test_sge_lock_simple PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_lock_multiple test_sge_lock_main.c test_sge_lock_multiple.c)
target_include_directories(test_sge_lock_multiple PRIVATE "./")
target_link_libraries(test_sge_lock_multiple PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_lock_fifo test_sge_lock_main.c test_sge_lock_fifo.c)
target_include_directories(test_sge_lock_fifo PRIVATE "./")
target_link_libraries(test_sge_lock_fifo PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_profiling test_sge_profiling.c)
target_include_directories(test_sge_profiling PRIVATE "./")
target_link_libraries(test_sge_profiling PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_recursive test_sge_lock_main.c test_sge_recursive.c)
target_include_directories(test_sge_recursive PRIVATE "./")
target_link_libraries(test_sge_recursive PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_sl test_sge_sl.c)
target_include_directories(test_sge_sl PRIVATE "./")
target_link_libraries(test_sge_sl PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_string test_sge_string.c)
target_include_directories(test_sge_string PRIVATE "./")
target_link_libraries(test_sge_string PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_tq test_sge_tq.c)
target_include_directories(test_sge_tq PRIVATE "./")
target_link_libraries(test_sge_tq PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_uidgid test_sge_uidgid.c)
target_include_directories(test_sge_uidgid PRIVATE "./")
target_link_libraries(test_sge_uidgid PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_thread_local test_thread_local.c)
target_include_directories(test_thread_local PRIVATE "./")
target_link_libraries(test_thread_local PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_threads test_threads.c)
target_include_directories(test_threads PRIVATE "./")
target_link_libraries(test_threads PRIVATE uti commlists ${SGE_LIBS})

if(INSTALL_SGE_TEST)
    install(TARGETS test_sge_bitfield DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_sge_deadlock DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_sge_dstring DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_sge_err DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_sge_lock_simple DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_sge_lock_multiple DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_sge_lock_fifo DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_sge_profiling DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_sge_recursive DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_sge_sl DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_sge_string DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_sge_tq DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_sge_uidgid DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_thread_local DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_threads DESTINATION testbin/${SGE_ARCH})
endif()
