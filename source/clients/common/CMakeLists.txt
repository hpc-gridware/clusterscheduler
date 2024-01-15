# source/clients/common
set(LIBRARY_NAME clientscommon)
set(LIBRARY_SOURCES
    "oge_client_print.cc"
    "oge_client_parse.cc"
    "oge_client_cqueue.cc"
    "oge_client_job.cc")
set(LIBRARY_INCLUDES "./" ${SGE_INCLUDES})

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES})
