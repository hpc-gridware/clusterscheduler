# source/libs/sgeobj
set(LIBRARY_NAME sgeobj)
set(LIBRARY_SOURCES
    config.c
    cull_parse_util.c
    parse.c
    sge_ack.c
    sge_advance_reservation.c
    sge_all_listsL.c
    sge_answer.c
    sge_attr.c
    sge_binding.c
    sge_calendar.c
    sge_centry.c
    sge_ckpt.c
    sge_conf.c
    sge_cqueue.c
    sge_cqueue_verify.c
    sge_cull_xml.c
    sge_eval_expression.c
    sge_event.c
    sge_feature.c
    sge_hgroup.c
    sge_host.c
    sge_href.c
    sge_id.c
    sge_ja_task.c
    sge_job.c
    sge_jsv.c
    sge_jsv_script.c
    sge_load.c
    sge_mailrec.c
    sge_manop.c
    sge_mesobj.c
    sge_object.c
    sge_order.c
    sge_pack.c
    sge_path_alias.c
    sge_pe.c
    sge_pe_task.c
    sge_proc.c
    sge_qinstance.c
    sge_qinstance_state.c
    sge_qinstance_type.c
    sge_qref.c
    sge_range.c
    sge_report.c
    sge_resource_quota.c
    sge_schedd_conf.c
    sge_sharetree.c
    sge_str.c
    # sge_sub_object_cpp.c
    sge_subordinate.c
    sge_suser.c
    sge_ulong.c
    sge_usage.c
    sge_userprj.c
    sge_userset.c
    sge_utility.c
    sge_var.c
    str2nm_converter.c
    cull/sge_sub_object.c)
set(LIBRARY_INCLUDES ${SGE_INCLUDES} "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${LIBRARY_INCLUDES})

# add_executable(test_sge_utility test_sge_utility.c) install(TARGETS
# test_sge_utility DESTINATION test/bin/${SGE_ARCH})
