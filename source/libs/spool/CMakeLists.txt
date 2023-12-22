# source/libs/spool
set(LIBRARY_NAME spool)
set(LIBRARY_SOURCES sge_dirent.cc sge_spooling.cc sge_spooling_utilities.cc)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
                                                  ${LIBRARY_INCLUDES})

add_executable(test_performance test_performance.cc)
target_include_directories(test_performance PRIVATE "./")
target_link_libraries(test_performance PRIVATE spool spoolloader spoold sgeobj cull comm uti commlists dl)

add_executable(test_spooling_mt test_spooling_mt.cc)
target_include_directories(test_spooling_mt PRIVATE "./")
target_link_libraries(test_spooling_mt PRIVATE spool spoolloader spoold sgeobj cull comm uti commlists dl)

add_executable(test_sge_spooling_utilities test_sge_spooling_utilities.cc)
target_include_directories(test_sge_spooling_utilities PRIVATE "./")
target_link_libraries(test_sge_spooling_utilities PRIVATE spool spoolloader spoold sched sgeobj cull comm uti commlists dl)

add_executable(test_sge_spooling test_sge_spooling.cc ../../common/sig_handlers.cc)
target_include_directories(test_sge_spooling PRIVATE "./")
target_link_libraries(test_sge_spooling PRIVATE spool spoolloader mir evc gdi spoold sgeobj cull comm uti commlists dl)

if(INSTALL_SGE_TEST)
    install(TARGETS test_performance DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_spooling_mt DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_sge_spooling_utilities DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_sge_spooling DESTINATION testbin/${SGE_ARCH})
endif()


add_subdirectory(berkeleydb)
# add_subdirectory(classic)
add_subdirectory(dynamic)
add_subdirectory(flatfile)
add_subdirectory(loader)
