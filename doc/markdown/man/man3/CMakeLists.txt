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

# gridengine/doc/markdown/man/man3

# we have pages referencing
# - drmaa_attributes.3
# - drmaa_jobcontrol.3
# - drmaa_jobtemplate.3
# - drmaa_misc.3
# - drmaa_session.3
# - drmaa_submit.3
# - drmaa_wait.3

set(ATTRIBUTES_PAGES drmaa_attributes drmaa_get_attribute_names drmaa_get_next_attr_name drmaa_get_num_attr_names
      drmaa_get_vector_attribute_names drmaa_release_attr_names)
build_markdown_man_from_template("3" "drmaa_attributes.include" ATTRIBUTES_PAGES)

set(JOBCONTROL_PAGES drmaa_control drmaa_jobcontrol drmaa_job_ps)
build_markdown_man_from_template("3" "drmaa_jobcontrol.include" JOBCONTROL_PAGES)

set(JOBTEMPLATE_PAGES drmaa_allocate_job_template drmaa_delete_job_template drmaa_get_attribute
      drmaa_get_next_attr_value drmaa_get_num_attr_values drmaa_get_vector_attribute drmaa_jobtemplate
      drmaa_release_attr_values drmaa_set_attribute drmaa_set_vector_attribute)
build_markdown_man_from_template("3" "drmaa_jobtemplate.include" JOBTEMPLATE_PAGES)

set(MISC_PAGES drmaa_get_contact drmaa_get_DRMAA_implementation drmaa_get_DRM_system drmaa_misc
      drmaa_strerror drmaa_version)
build_markdown_man_from_template("3" "drmaa_misc.include" MISC_PAGES)

set(SESSION_PAGES drmaa_exit drmaa_init drmaa_session)
build_markdown_man_from_template("3" "drmaa_session.include" SESSION_PAGES)

set(SUBMIT_PAGES drmaa_get_next_job_id drmaa_get_num_job_ids drmaa_release_job_ids drmaa_run_bulk_jobs
      drmaa_run_job drmaa_submit)
build_markdown_man_from_template("3" "drmaa_submit.include" SUBMIT_PAGES)

set(WAIT_PAGES drmaa_synchronize drmaa_wait drmaa_wcoredump drmaa_wexitstatus drmaa_wifaborted
      drmaa_wifexited drmaa_wifsignaled drmaa_wtermsig)
build_markdown_man_from_template("3" "drmaa_wait.include" WAIT_PAGES)

set(PAGES jsv_script_interface)
build_markdown_man("3" PAGES)

# target for building all troff man pages from section 3
# @todo is there a better way of listing them? Didn't find an easily comprehensible way to use foreach().
add_custom_target(troffman3 ALL DEPENDS
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_allocate_job_template.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_attributes.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_control.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_delete_job_template.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_exit.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_get_attribute.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_get_attribute_names.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_get_contact.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_get_DRMAA_implementation.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_get_DRM_system.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_get_next_attr_name.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_get_next_attr_value.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_get_next_job_id.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_get_num_attr_names.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_get_num_attr_values.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_get_num_job_ids.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_get_vector_attribute.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_get_vector_attribute_names.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_init.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_jobcontrol.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_job_ps.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_jobtemplate.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_misc.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_release_attr_names.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_release_attr_values.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_release_job_ids.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_run_bulk_jobs.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_run_job.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_session.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_set_attribute.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_set_vector_attribute.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_strerror.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_submit.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_synchronize.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_version.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_wait.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_wcoredump.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_wexitstatus.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_wifaborted.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_wifexited.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_wifsignaled.3
      ${CMAKE_CURRENT_BINARY_DIR}/drmaa_wtermsig.3
      ${CMAKE_CURRENT_BINARY_DIR}/jsv_script_interface.3
)

# installation
if (INSTALL_SGE_DOC)
   foreach (PAGE ${ATTRIBUTES_PAGES} ${JOBCONTROL_PAGES} ${JOBTEMPLATE_PAGES} ${MISC_PAGES} ${SESSION_PAGES} ${SUBMIT_PAGES} ${WAIT_PAGES} ${PAGES})
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PAGE}.3 DESTINATION man/man3)
   endforeach ()
endif ()
