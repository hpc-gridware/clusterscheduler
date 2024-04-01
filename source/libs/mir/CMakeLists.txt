# source/libs/mir
set(LIBRARY_NAME mir)
set(LIBRARY_SOURCES
      sge_host_mirror.cc
      sge_ja_task_mirror.cc
      sge_job_mirror.cc
      sge_mirror.cc
      sge_pe_task_mirror.cc
      sge_queue_mirror.cc
      sge_sched_conf_mirror.cc
      sge_sharetree_mirror.cc)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
      ${LIBRARY_INCLUDES})

