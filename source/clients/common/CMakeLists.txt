# source/clients/common
set(LIBRARY_NAME clientscommon)
set(LIBRARY_SOURCES
    "qstat_printing.c"
    "read_defaults.c"
    "sge_centry_qconf.c"
    "sge_client_ijs.c"
    "sge_cqueue_qconf.c"
    "sge_cqueue_qstat.c"
    "sge_hgroup_qconf.c"
    "sge_qhost.c"
    "sge_qquota.c"
    "sge_qrsub.c"
    "sge_qstat.c"
    "sge_resource_quota_qconf.c"
    "show_job.c"
    "write_job_defaults.c")
set(LIBRARY_INCLUDES "./" ${SGE_INCLUDES})

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES})
