#___INFO__MARK_BEGIN_NEW__
###########################################################################
#  
#  Copyright 2023-2024 HPC-Gridware GmbH
#  
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#  
#      http://www.apache.org/licenses/LICENSE-2.0
#  
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#  
###########################################################################
#___INFO__MARK_END_NEW__

# source/libs/sgeobj

# Generate CULL header files from JSON files
# This is a custom target that is not triggered automatically and only available with the gcs-extensions
# Automatic generation of header files is not supported because it would break the build via testsuite.
if (PROJECT_FEATURES MATCHES "gcs-extensions")
    add_custom_target(cull_header
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMAND gen_types json CULL cull
            COMMENT "Generating CULL header files"
    )
endif()

# Generate LWDB header files from JSON files
# This is a custom target that is not triggered automatically and only available with the gcs-extensions
# Automatic generation of header files is not supported because it would break the build via testsuite.
if (PROJECT_FEATURES MATCHES "gcs-extensions")
    if (1)
        # manual way to generate the header files
        add_custom_target(lwdb_header
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMAND gen_types json LWDB lwdb
                COMMENT "Generating LWDB header files"
        )
    else()
        # automatic way to generate the header files (will break parallel build with TS)
        file(GLOB INPUT_FILES ${CMAKE_CURRENT_SOURCE_DIR}/json/*.json)
        file(GLOB OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lwdb)
        set(OUTPUT_FILES
                ${OUTPUT_DIR}/ocs_all_attributes.h
                # no need to list other files here, they are included in the file above
        )
        add_custom_command(
                OUTPUT ${OUTPUT_FILES}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMAND gen_types json LWDB lwdb
                DEPENDS ${INPUT_FILES} gen_types
                COMMENT "Generating LWDB header files"
        )
        add_custom_target(lwdb_header
                DEPENDS ${OUTPUT_FILES})
    endif()
endif()

set(LIBRARY_NAME sgeobj)
set(LIBRARY_SOURCES
      config.cc
      cull_parse_util.cc
      ocs_binding_io.cc
      ocs_BindingFinder.cc
      ocs_Category.cc
      ocs_DataStore.cc
      ocs_HostTopology.cc
      ocs_Session.cc
      ocs_Version.cc
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
      sge_resource_quota_service.cc
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
      ../../../../${PROJECT_FEATURES}/source/libs/sgeobj/ocs_RequestLimits.cc
      cull/sge_sub_object.cc
)
set(LIBRARY_INCLUDES ${SGE_INCLUDES} "./")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES} ${OUTPUT_FILES})
#if (PROJECT_FEATURES MATCHES "gcs-extensions")
#    add_dependencies(${LIBRARY_NAME} cull_header)
#endif ()
target_include_directories(${LIBRARY_NAME} PUBLIC ${LIBRARY_INCLUDES})

#add_subdirectory(cpp)
