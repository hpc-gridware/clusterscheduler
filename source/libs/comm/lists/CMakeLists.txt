# source/libs/comm/lists
set(LIBRARY_NAME commlists)
set(LIBRARY_SOURCES
        cl_errors.cc
        cl_log_list.cc
        cl_parameter_list.cc
        cl_raw_list.cc
        cl_string_list.cc
        cl_thread.cc
        cl_thread_list.cc
        cl_util.cc)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES} ${LIBRARY_INCLUDES})
target_link_libraries(test_cl_com_parameter_list PRIVATE ${SGE_LIBS})
