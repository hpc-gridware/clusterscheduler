# common
set(LIBRARY_NAME common)
set(LIBRARY_SOURCES
      execution_states.cc
      parse_job_cull.cc
      parse_qsub.cc
      sge_ijs_comm.cc
      sge_ijs_threads.cc
      sge_options.cc
      sig_handlers.cc
      usage.cc)
set(LIBRARY_INCLUDES ${SGE_INCLUDES} "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${LIBRARY_INCLUDES})
