# source/libs/sgeobj
set(LIBRARY_NAME sgeobj)
set(LIBRARY_SOURCES
      config.cc
      cull_parse_util.cc
      parse.cc
      sge_ack.cc
      sge_advance_reservation.cc
      sge_all_listsL.cc
      sge_answer.cc
      sge_attr.cc
      sge_binding.cc
      sge_calendar.cc
      sge_centry.cc
      sge_centry_rsmap.cc
      sge_ckpt.cc
      sge_conf.cc
      sge_cqueue.cc
      sge_cqueue_verify.cc
      sge_cull_xml.cc
      sge_daemonize.cc
      sge_eval_expression.cc
      sge_event.cc
      sge_feature.cc
      sge_grantedres.cc
      sge_hgroup.cc
      sge_host.cc
      sge_href.cc
      sge_id.cc
      sge_ja_task.cc
      sge_job.cc
      sge_jsv.cc
      sge_jsv_script.cc
      sge_load.cc
      sge_mailrec.cc
      sge_manop.cc
      sge_mesobj.cc
      sge_object.cc
      sge_order.cc
      sge_pack.cc
      sge_path_alias.cc
      sge_pe.cc
      sge_pe_task.cc
      sge_proc.cc
      sge_qinstance.cc
      sge_qinstance_state.cc
      sge_qinstance_type.cc
      sge_qref.cc
      sge_range.cc
      sge_report.cc
      sge_resource_quota.cc
      sge_schedd_conf.cc
      sge_sharetree.cc
      sge_str.cc
      sge_subordinate.cc
      sge_suser.cc
      sge_ulong.cc
      sge_usage.cc
      sge_userprj.cc
      sge_userset.cc
      sge_utility.cc
      sge_var.cc
      str2nm_converter.cc
      cull/sge_sub_object.cc)
set(LIBRARY_INCLUDES ${SGE_INCLUDES} "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${LIBRARY_INCLUDES})

add_executable(test_eval_expression test_eval_expression.cc)
target_include_directories(test_eval_expression PRIVATE "./")
target_link_libraries(test_eval_expression PRIVATE sgeobj gdi cull commlists uti ${SGE_LIBS})

add_executable(test_obj_performance test_obj_performance.cc)
target_include_directories(test_obj_performance PRIVATE "./")
target_link_libraries(test_obj_performance PRIVATE sgeobj gdi cull uti commlists ${SGE_LIBS})

#add_executable(test_sge_attr test_sge_attr.cc)
#target_include_directories(test_sge_attr PRIVATE "./")
#target_link_libraries(test_sge_attr PRIVATE sgeobj cull commlists uti ${SGE_LIBS})

#add_executable(test_sge_fgl test_sge_fgl.cc)
#target_include_directories(test_sge_fgl PRIVATE "./")
#target_link_libraries(test_sge_fgl PRIVATE sgeobj cull commlists uti ${SGE_LIBS})

add_executable(test_sge_object test_sge_object.cc)
target_include_directories(test_sge_object PRIVATE "./")
target_link_libraries(test_sge_object PRIVATE sgeobj cull comm uti commlists ${SGE_LIBS})

add_executable(test_sge_range test_sge_range.cc)
target_include_directories(test_sge_range PRIVATE "./")
target_link_libraries(test_sge_range PRIVATE sgeobj cull comm uti commlists ${SGE_LIBS})

add_executable(test_sge_resource_quota test_sge_resource_quota.cc)
target_include_directories(test_sge_resource_quota PRIVATE "./")
target_link_libraries(test_sge_resource_quota PRIVATE sgeobj cull comm uti commlists ${SGE_LIBS})

add_executable(test_sge_schedd_conf test_sge_schedd_conf.cc)
target_include_directories(test_sge_schedd_conf PRIVATE "./")
target_link_libraries(test_sge_schedd_conf PRIVATE sgeobj cull comm uti commlists ${SGE_LIBS})

add_executable(test_sge_utility test_sge_utility.cc)
target_include_directories(test_sge_utility PRIVATE "./")
target_link_libraries(test_sge_utility PRIVATE sgeobj cull comm uti commlists ${SGE_LIBS})

if (INSTALL_SGE_TEST)
   install(TARGETS test_eval_expression DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_obj_performance DESTINATION testbin/${SGE_ARCH})
   #    install(TARGETS test_sge_attr DESTINATION testbin/${SGE_ARCH})
   #    install(TARGETS test_sge_fgl DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sge_object DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sge_range DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sge_resource_quota DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sge_schedd_conf DESTINATION testbin/${SGE_ARCH})
   install(TARGETS test_sge_utility DESTINATION testbin/${SGE_ARCH})
endif ()

#add_subdirectory(cpp)
