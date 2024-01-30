# source/libs/uti
set(LIBRARY_NAME uti)
set(LIBRARY_SOURCES
        config_file.cc
        setup_path.cc
        sge_afsutil.cc
        sge_arch.cc
        sge_binding_hlp.cc
        sge_binding_parse.cc
        sge_bitfield.cc
        sge_bootstrap.cc
        sge_csp_path.cc
        sge_dstring.cc
        sge_edit.cc
        sge_env.cc
        sge_err.cc
        sge_error_class.cc
        sge_fgl.cc
        sge_getloadavg.cc
        sge_hostname.cc
        sge_htable.cc
        sge_io.cc
        sge_language.cc
        sge_loadmem.cc
        sge_lock.cc
        sge_lock_fifo.cc
        sge_log.cc
        sge_monitor.cc
        sge_mtutil.cc
        sge_nprocs.cc
        sge_os.cc
        sge_parse_args.cc
        sge_parse_num_par.cc
        sge_profiling.cc
        sge_prog.cc
        sge_pty.cc
        sge_rmon_macros.cc
        sge_rmon_monitoring_level.cc
        sge_signal.cc
        sge_sl.cc
        sge_smf.cc
        sge_spool.cc
        sge_stdio.cc
        sge_stdlib.cc
        sge_string.cc
        sge_thread_ctrl.cc
        sge_time.cc
        sge_tmpnam.cc
      oge_topology.cc
        sge_tq.cc
        sge_uidgid.cc
        sge_unistd.cc)
set(LIBRARY_INCLUDES "./" ${SGE_INCLUDES})

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${LIBRARY_INCLUDES})

add_executable(test_sge_bitfield test_sge_bitfield.cc)
target_include_directories(test_sge_bitfield PRIVATE "./")
target_link_libraries(test_sge_bitfield PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_deadlock test_sge_lock_main.cc test_sge_deadlock.cc)
target_include_directories(test_sge_deadlock PRIVATE "./")
target_link_libraries(test_sge_deadlock PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_dstring test_sge_dstring.cc)
target_include_directories(test_sge_dstring PRIVATE "./")
target_link_libraries(test_sge_dstring PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_err test_sge_err.cc)
target_include_directories(test_sge_err PRIVATE "./")
target_link_libraries(test_sge_err PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_lock_simple test_sge_lock_main.cc test_sge_lock_simple.cc)
target_include_directories(test_sge_lock_simple PRIVATE "./")
target_link_libraries(test_sge_lock_simple PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_lock_multiple test_sge_lock_main.cc test_sge_lock_multiple.cc)
target_include_directories(test_sge_lock_multiple PRIVATE "./")
target_link_libraries(test_sge_lock_multiple PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_lock_fifo test_sge_lock_main.cc test_sge_lock_fifo.cc)
target_include_directories(test_sge_lock_fifo PRIVATE "./")
target_link_libraries(test_sge_lock_fifo PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_profiling test_sge_profiling.cc)
target_include_directories(test_sge_profiling PRIVATE "./")
target_link_libraries(test_sge_profiling PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_recursive test_sge_lock_main.cc test_sge_recursive.cc)
target_include_directories(test_sge_recursive PRIVATE "./")
target_link_libraries(test_sge_recursive PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_sl test_sge_sl.cc)
target_include_directories(test_sge_sl PRIVATE "./")
target_link_libraries(test_sge_sl PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_string test_sge_string.cc)
target_include_directories(test_sge_string PRIVATE "./")
target_link_libraries(test_sge_string PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_tq test_sge_tq.cc)
target_include_directories(test_sge_tq PRIVATE "./")
target_link_libraries(test_sge_tq PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_sge_uidgid test_sge_uidgid.cc)
target_include_directories(test_sge_uidgid PRIVATE "./")
target_link_libraries(test_sge_uidgid PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_thread_local test_thread_local.cc)
target_include_directories(test_thread_local PRIVATE "./")
target_link_libraries(test_thread_local PRIVATE uti commlists ${SGE_LIBS})

add_executable(test_threads test_threads.cc)
target_include_directories(test_threads PRIVATE "./")
target_link_libraries(test_threads PRIVATE uti commlists ${SGE_LIBS})

if (INSTALL_SGE_TEST)
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
endif ()
