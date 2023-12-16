# source/libs/gdi
set(LIBRARY_NAME gdi)
set(LIBRARY_SOURCES
    pack_job_delivery.c
    qm_name.c
    sge_gdi2.c
    sge_gdi_ctx.c
    sge_gdi_packet.c
    sge_gdi_packet_internal.c
    sge_gdi_packet_pb_cull.c
    sge_qexec.c
    sge_qtcsh.c
    sge_security.c
    version.c)
set(LIBRARY_INCLUDES ${SGE_INCLUDES} "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${LIBRARY_INCLUDES})
