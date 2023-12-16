# source/libs/mir
set(LIBRARY_NAME mir)
set(LIBRARY_SOURCES
    sge_host_mirror.c
    sge_ja_task_mirror.c
    sge_job_mirror.c
    sge_mirror.c
    sge_pe_task_mirror.c
    sge_queue_mirror.c
    sge_sched_conf_mirror.c
    sge_sharetree_mirror.c)
set(LIBRARY_INCLUDES "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
                                                  ${LIBRARY_INCLUDES})
