# common
set(LIBRARY_NAME common)
set(LIBRARY_SOURCES
    "execution_states.c"
    "parse_job_cull.c"
    "parse_qsub.c"
    "sge_ijs_comm.c"
    "sge_ijs_threads.c"
    "sge_mt_init.c"
    "sge_options.c"
    "sig_handlers.c"
    "usage.c")
set(LIBRARY_INCLUDES ${SGE_INCLUDES} "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${LIBRARY_INCLUDES})
