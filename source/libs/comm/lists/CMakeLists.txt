# source/libs/comm/lists
set(LIBRARY_NAME commlists)
set(LIBRARY_SOURCES
    cl_errors.c
    cl_log_list.c
    cl_parameter_list.c
    cl_raw_list.c
    cl_string_list.c
    cl_thread.c
    cl_thread_list.c
    cl_util.c)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES} ${LIBRARY_INCLUDES})
target_link_libraries(test_cl_com_parameter_list PRIVATE ${SGE_LIBS})
