# source/libs/comm
set(LIBRARY_NAME comm)
set(LIBRARY_SOURCES
    cl_app_message_queue.c
    cl_application_error_list.c
    cl_commlib.c
    cl_communication.c
    cl_connection_list.c
    cl_endpoint_list.c
    cl_fd_list.c
    cl_handle_list.c
    cl_host_alias_list.c
    cl_host_list.c
    cl_message_list.c
    cl_ssl_framework.c
    cl_tcp_framework.c
    cl_xml_parsing.c)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
                                                  ${LIBRARY_INCLUDES})

add_executable(test_cl_com_parameter_list test_cl_com_parameter_list.c)
target_include_directories(test_cl_com_parameter_list PRIVATE "./")
target_link_libraries(test_cl_com_parameter_list PRIVATE comm uti commlists)

add_executable(test_cl_commlib test_cl_commlib.c)
target_include_directories(test_cl_commlib PRIVATE "./")
target_link_libraries(test_cl_commlib PRIVATE comm uti commlists)

add_executable(test_cl_commlib_client test_cl_commlib_client.c)
target_include_directories(test_cl_commlib_client PRIVATE "./")
target_link_libraries(test_cl_commlib_client PRIVATE comm uti commlists)

add_executable(test_cl_endpoint_list test_cl_endpoint_list.c)
target_include_directories(test_cl_endpoint_list PRIVATE "./")
target_link_libraries(test_cl_endpoint_list PRIVATE comm uti commlists)

add_executable(test_cl_hostalias_list test_cl_hostalias_list.c)
target_include_directories(test_cl_hostalias_list PRIVATE "./")
target_link_libraries(test_cl_hostalias_list PRIVATE comm uti commlists)

add_executable(test_cl_hostname_list test_cl_hostname_list.c)
target_include_directories(test_cl_hostname_list PRIVATE "./")
target_link_libraries(test_cl_hostname_list PRIVATE comm uti commlists)

add_executable(test_cl_issue_2747_client test_cl_issue_2747_client.c)
target_include_directories(test_cl_issue_2747_client PRIVATE "./")
target_link_libraries(test_cl_issue_2747_client PRIVATE comm uti commlists)

add_executable(test_cl_issue_2747_server test_cl_issue_2747_server.c)
target_include_directories(test_cl_issue_2747_server PRIVATE "./")
target_link_libraries(test_cl_issue_2747_server PRIVATE comm uti commlists)

#@todo: outdated, too few arguments to cl_thread_list_create_thread()
#add_executable(test_cl_tcp_framework test_cl_tcp_framework.c)
#target_include_directories(test_cl_tcp_framework PRIVATE "./")
#target_link_libraries(test_cl_tcp_framework PRIVATE comm uti commlists)

add_executable(test_commlib_issue test_commlib_issue.c)
target_include_directories(test_commlib_issue PRIVATE "./")
target_link_libraries(test_commlib_issue PRIVATE comm uti commlists)

add_executable(test_debug_client test_debug_client.c)
target_include_directories(test_debug_client PRIVATE "./")
target_link_libraries(test_debug_client PRIVATE comm uti commlists)

#add_executable(test_dummy test_dummy.c)
#target_include_directories(test_dummy PRIVATE "./")
#target_link_libraries(test_dummy PRIVATE comm uti commlists)

add_executable(test_general test_general.c)
target_include_directories(test_general PRIVATE "./")
target_link_libraries(test_general PRIVATE comm uti commlists)

add_executable(test_issues test_issues.c)
target_include_directories(test_issues PRIVATE "./")
target_link_libraries(test_issues PRIVATE comm uti commlists)

add_executable(test_multithread_io test_multithread_io.c)
target_include_directories(test_multithread_io PRIVATE "./")
target_link_libraries(test_multithread_io PRIVATE comm uti commlists)

add_executable(test_server_connection test_server_connection.c)
target_include_directories(test_server_connection PRIVATE "./")
target_link_libraries(test_server_connection PRIVATE comm uti commlists)

add_executable(test_status_info test_status_info.c)
target_include_directories(test_status_info PRIVATE "./")
target_link_libraries(test_status_info PRIVATE comm uti commlists)

add_executable(test_thread_throughput test_thread_throughput.c)
target_include_directories(test_thread_throughput PRIVATE "./")
target_link_libraries(test_thread_throughput PRIVATE comm uti commlists)

add_executable(test_virtual_event_client test_virtual_event_client.c)
target_include_directories(test_virtual_event_client PRIVATE "./")
target_link_libraries(test_virtual_event_client PRIVATE comm uti commlists)

add_executable(test_virtual_gdi_client test_virtual_gdi_client.c)
target_include_directories(test_virtual_gdi_client PRIVATE "./")
target_link_libraries(test_virtual_gdi_client PRIVATE comm uti commlists)

add_executable(test_virtual_qmaster test_virtual_qmaster.c)
target_include_directories(test_virtual_qmaster PRIVATE "./")
target_link_libraries(test_virtual_qmaster PRIVATE comm uti commlists)

if(INSTALL_SGE_TEST)
    install(TARGETS test_cl_com_parameter_list DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_cl_commlib DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_cl_commlib_client DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_cl_endpoint_list DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_cl_hostalias_list DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_cl_hostname_list DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_cl_issue_2747_client DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_cl_issue_2747_server DESTINATION testbin/${SGE_ARCH})
#    install(TARGETS test_cl_tcp_framework DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_commlib_issue DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_debug_client DESTINATION testbin/${SGE_ARCH})
#    install(TARGETS test_dummy DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_general DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_issues DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_multithread_io DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_server_connection DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_status_info DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_thread_throughput DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_virtual_event_client DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_virtual_gdi_client DESTINATION testbin/${SGE_ARCH})
    install(TARGETS test_virtual_qmaster DESTINATION testbin/${SGE_ARCH})
endif()

add_subdirectory(lists)
