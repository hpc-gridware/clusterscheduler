# source/clients/common
set(LIBRARY_NAME clientscommon)
set(LIBRARY_SOURCES
    "qstat_printing.cc"
    "read_defaults.cc"
    "sge_centry_qconf.cc"
    "sge_client_ijs.cc"
    "sge_cqueue_qconf.cc"
    "sge_cqueue_qstat.cc"
    "sge_hgroup_qconf.cc"
    "sge_qhost.cc"
    "sge_qquota.cc"
    "sge_qrsub.cc"
    "sge_qstat.cc"
    "sge_resource_quota_qconf.cc"
    "show_job.cc"
    "write_job_defaults.cc")
set(LIBRARY_INCLUDES "./" ${SGE_INCLUDES})

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES})
