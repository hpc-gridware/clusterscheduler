#___INFO__MARK_BEGIN_NEW__
###########################################################################
#  
#  Copyright 2023-2024 HPC-Gridware GmbH
#  
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#  
#      http://www.apache.org/licenses/LICENSE-2.0
#  
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#  
###########################################################################
#___INFO__MARK_END_NEW__

# source/libs/comm
set(LIBRARY_NAME comm)
set(LIBRARY_SOURCES
        cl_app_message_queue.cc
        cl_application_error_list.cc
        cl_commlib.cc
        cl_communication.cc
        cl_connection_list.cc
        cl_endpoint_list.cc
        cl_fd_list.cc
        cl_handle_list.cc
        cl_host_alias_list.cc
        cl_host_list.cc
        cl_message_list.cc
        cl_ssl_framework.cc
        cl_tcp_framework.cc
        cl_xml_parsing.cc)
set(LIBRARY_INCLUDES "./")

add_compile_options(-Wno-deprecated-declarations)

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
      ${LIBRARY_INCLUDES})

add_executable(test_cl_com_parameter_list test_cl_com_parameter_list.cc)
target_include_directories(test_cl_com_parameter_list PRIVATE "./")
target_link_libraries(test_cl_com_parameter_list PRIVATE comm commlists uti)

add_executable(test_cl_commlib test_cl_commlib.cc)
target_include_directories(test_cl_commlib PRIVATE "./")
target_link_libraries(test_cl_commlib PRIVATE comm commlists uti ${SGE_LIBS})

add_executable(test_cl_commlib_client test_cl_commlib_client.cc)
target_include_directories(test_cl_commlib_client PRIVATE "./")
target_link_libraries(test_cl_commlib_client PRIVATE comm commlists uti ${SGE_LIBS})

add_executable(test_cl_endpoint_list test_cl_endpoint_list.cc)
target_include_directories(test_cl_endpoint_list PRIVATE "./")
target_link_libraries(test_cl_endpoint_list PRIVATE comm commlists uti ${SGE_LIBS})

add_executable(test_cl_hostalias_list test_cl_hostalias_list.cc)
target_include_directories(test_cl_hostalias_list PRIVATE "./")
target_link_libraries(test_cl_hostalias_list PRIVATE comm commlists uti ${SGE_LIBS})

add_executable(test_cl_hostname_list test_cl_hostname_list.cc)
target_include_directories(test_cl_hostname_list PRIVATE "./")
target_link_libraries(test_cl_hostname_list PRIVATE comm commlists uti ${SGE_LIBS})

add_executable(test_cl_issue_2747_client test_cl_issue_2747_client.cc)
target_include_directories(test_cl_issue_2747_client PRIVATE "./")
target_link_libraries(test_cl_issue_2747_client PRIVATE comm commlists uti ${SGE_LIBS})

add_executable(test_cl_issue_2747_server test_cl_issue_2747_server.cc)
target_include_directories(test_cl_issue_2747_server PRIVATE "./")
target_link_libraries(test_cl_issue_2747_server PRIVATE comm commlists uti ${SGE_LIBS})

#@todo: outdated, too few arguments to cl_thread_list_create_thread()
#add_executable(test_cl_tcp_framework test_cl_tcp_framework.c)
#target_include_directories(test_cl_tcp_framework PRIVATE "./")
#target_link_libraries(test_cl_tcp_framework PRIVATE comm commlists uti)

add_executable(test_commlib_issue test_commlib_issue.cc)
target_include_directories(test_commlib_issue PRIVATE "./")
target_link_libraries(test_commlib_issue PRIVATE comm commlists uti ${SGE_LIBS})

add_executable(test_debug_client test_debug_client.cc)
target_include_directories(test_debug_client PRIVATE "./")
target_link_libraries(test_debug_client PRIVATE comm commlists uti ${SGE_LIBS})

#add_executable(test_dummy test_dummy.c)
#target_include_directories(test_dummy PRIVATE "./")
#target_link_libraries(test_dummy PRIVATE comm commlists uti)

add_executable(test_general test_general.cc)
target_include_directories(test_general PRIVATE "./")
target_link_libraries(test_general PRIVATE comm commlists uti)

add_executable(test_issues test_issues.cc)
target_include_directories(test_issues PRIVATE "./")
target_link_libraries(test_issues PRIVATE comm commlists uti ${SGE_LIBS})

add_executable(test_multithread_io test_multithread_io.cc)
target_include_directories(test_multithread_io PRIVATE "./")
target_link_libraries(test_multithread_io PRIVATE comm commlists uti ${SGE_LIBS})

add_executable(test_server_connection test_server_connection.cc)
target_include_directories(test_server_connection PRIVATE "./")
target_link_libraries(test_server_connection PRIVATE comm commlists uti ${SGE_LIBS})

add_executable(test_status_info test_status_info.cc)
target_include_directories(test_status_info PRIVATE "./")
target_link_libraries(test_status_info PRIVATE comm commlists uti ${SGE_LIBS})

add_executable(test_thread_throughput test_thread_throughput.cc)
target_include_directories(test_thread_throughput PRIVATE "./")
target_link_libraries(test_thread_throughput PRIVATE comm commlists uti ${SGE_LIBS})

add_executable(test_virtual_event_client test_virtual_event_client.cc)
target_include_directories(test_virtual_event_client PRIVATE "./")
target_link_libraries(test_virtual_event_client PRIVATE comm commlists uti ${SGE_LIBS})

add_executable(test_virtual_gdi_client test_virtual_gdi_client.cc)
target_include_directories(test_virtual_gdi_client PRIVATE "./")
target_link_libraries(test_virtual_gdi_client PRIVATE comm commlists uti ${SGE_LIBS})

add_executable(test_virtual_qmaster test_virtual_qmaster.cc)
target_include_directories(test_virtual_qmaster PRIVATE "./")
target_link_libraries(test_virtual_qmaster PRIVATE comm commlists uti ${SGE_LIBS})

if (INSTALL_SGE_TEST)
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
endif ()

add_subdirectory(lists)
