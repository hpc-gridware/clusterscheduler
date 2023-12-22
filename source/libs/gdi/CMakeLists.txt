# source/libs/gdi
set(LIBRARY_NAME gdi)
set(LIBRARY_SOURCES
    pack_job_delivery.cc
    qm_name.cc
    sge_gdi2.cc
    sge_gdi_ctx.cc
    sge_gdi_packet.cc
    sge_gdi_packet_internal.cc
    sge_gdi_packet_pb_cull.cc
    sge_qexec.cc
    sge_qtcsh.cc
    sge_security.cc
    version.cc)
set(LIBRARY_INCLUDES ${SGE_INCLUDES} "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${LIBRARY_INCLUDES})
