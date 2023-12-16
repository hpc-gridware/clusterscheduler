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

add_subdirectory(lists)
