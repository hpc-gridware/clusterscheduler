/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

#include <sstream>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_time.h"

#include "cull/cull.h"

#include "sgeobj/ocs_BindingIo.h"
#include "sgeobj/ocs_GrantedResources.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_mesobj.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_path_alias.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_grantedres.h"

#include "parse_qsub.h"
#include "msg_clients_common.h"
#include "sgeobj/cull_parse_util.h"
#include "qstat/job/ocs_QStatJobViewBase.h"


void ocs::QStatJobViewBase::show_job(std::ostream &os, const lListElem *job, int flags) {
   DENTER(TOP_LAYER);

   const int left_width_short = 19;
   const int mid_width = 11;
   const int left_width = left_width_short + mid_width + 2;
   const char *delis[3] = {nullptr, ",", "\n"};

   if (!job) {
      DRETURN_VOID;
   }

   report_job_id(os, job, flags);
   report_category_id(os, job);
   report_exec_file(os, job);
   report_submission_time(os, job);
   report_deadline_time(os, job);
   report_submit_cmd_line(os, job);
   report_effective_submit_cmd_line(os, job);
   report_ownership(os, job);
   report_env_core(os, job);
   report_execution_time(os, job);
   report_account(os, job);
   report_checkpoint(os, job);
   report_cwd(os, job);
   report_path_aliases(os, job);
   report_directive_prefix(os, job);
   report_stdin_path_list(os, job);
   report_stdout_path_list(os, job);
   report_stderr_path_list(os, job);
   report_reserve(os, job);
   report_merge_stderr(os, job);
   report_request_set_list(os, job);
   report_mail_options(os, job);
   report_mail_list(os, job);
   report_notify(os, job);
   report_name(os, job);
   report_priority(os, job);
   report_job_share(os, job);
   report_restart(os, job);
   report_shell_list(os, job);
   report_env_list(os, job);
   report_verify(os, job);
   report_job_args(os, job);



   if (lGetPosViaElem(job, JB_qs_args, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_qs_args); list != nullptr || (flags & FLG_QALTER)) {
         std::ostringstream ss_list;
         int fields[] = {ST_name, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", "qs_args:", left_width, ss_list.str());
      }
   }

   if (lGetPosViaElem(job, JB_job_identifier_list, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_job_identifier_list)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_number, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", "job_identifier_list:", left_width, ss_list.str());
      }
   }

   if (lGetPosViaElem(job, JB_script_size, SGE_NO_ABORT) >= 0) {
      if (const uint32_t size = lGetUlong(job, JB_script_size)) {
         os << std::format("{:<{}} {}", "script_size:", left_width, size) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_script_file, SGE_NO_ABORT) >= 0) {
      if (const char *file = lGetString(job, JB_script_file)) {
         os << std::format("{:<{}} {}", "script_file:", left_width, file) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_script_ptr, SGE_NO_ABORT) >= 0) {
      if (const char *script = lGetString(job, JB_script_ptr)) {
         os << std::format("{:<{}} {}", "script_ptr:\n", left_width, script) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_pe, SGE_NO_ABORT) >= 0) {
      if (const char *pe = lGetString(job, JB_pe)) {
         dstring range_string = DSTRING_INIT;

         range_list_print_to_string(lGetList(job, JB_pe_range), &range_string, true, false, false);
         std::stringstream ss_pe_details;
         ss_pe_details << pe << " range: " << sge_dstring_get_string(&range_string);
         sge_dstring_free(&range_string);

         os << std::format("{:<{}} {}", "parallel_environment:", left_width, ss_pe_details.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_jid_request_list, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_jid_request_list)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_name, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", "jid_request_list (req):", left_width, ss_list.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_jid_predecessor_list, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_jid_predecessor_list)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_number, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", "jid_predecessor_list:", left_width, ss_list.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_jid_successor_list, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_jid_successor_list)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_number, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", "jid_successor_list:", left_width, ss_list.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_ja_ad_request_list, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_ja_ad_request_list)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_name, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", "ja_ad_redecessor_list (req):", left_width, ss_list.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_ja_ad_predecessor_list, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_ja_ad_predecessor_list)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_number, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", "ja_ad_predecessor_list:", left_width, ss_list.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_ja_ad_successor_list, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_ja_ad_successor_list)) {
         std::ostringstream ss_list;
         int fields[] = {JRE_job_number, 0};
         delis[0] = "";
         uni_print_list(ss_list, list, fields, delis, 0);
         os << std::format("{:<{}} {}", "ja_ad_successor_list:", left_width, ss_list.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_verify_suitable_queues, SGE_NO_ABORT) >= 0) {
      if (const uint32_t vsq = lGetUlong(job, JB_verify_suitable_queues)) {
         os << std::format("{:<{}} {}", "verify_suitable_queues:", left_width, vsq) << "\n";
      }
   }

   DSTRING_STATIC(dstr, 128);
   if (lGetPosViaElem(job, JB_soft_wallclock_gmt, SGE_NO_ABORT) >= 0) {
      if (uint64_t time_value = lGetUlong64(job, JB_soft_wallclock_gmt)) {
         os << std::format("{:<{}} {}", "soft_wallclock_gmt:", left_width, sge_ctime64(time_value, &dstr)) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_hard_wallclock_gmt, SGE_NO_ABORT) >= 0) {
      if (uint64_t time_value = lGetUlong64(job, JB_hard_wallclock_gmt)) {
         os << std::format("{:<{}} {}", "hard_wallclock_gmt:", left_width, sge_ctime64(time_value, &dstr)) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_version, SGE_NO_ABORT) >= 0) {
      if (uint32_t version = lGetUlong(job, JB_version)) {
         os << std::format("{:<{}} {}", "version:", left_width, version) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_override_tickets, SGE_NO_ABORT) >= 0) {
      if (uint32_t tickets = lGetUlong(job, JB_override_tickets)) {
         os << std::format("{:<{}} {}", "override_tickets:", left_width, tickets) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_project, SGE_NO_ABORT) >= 0) {
      if (const char *project = lGetString(job, JB_project)) {
         os << std::format("{:<{}} {}", "project:", left_width, project) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_department, SGE_NO_ABORT) >= 0) {
      if (const char *dept = lGetString(job, JB_department)) {
         os << std::format("{:<{}} {}", "department:", left_width, dept) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_ar, SGE_NO_ABORT) >= 0) {
      if (uint32_t ar_id = lGetUlong(job, JB_ar)) {
         os << std::format("{:<{}} {}", "ar_id:", left_width, ar_id) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_sync_options, SGE_NO_ABORT) >= 0) {
      if (lGetUlong(job, JB_sync_options)) {
         std::string sync_flags =  job_get_sync_options_string(job);
         os << std::format("{:<{}} {}", "sync_options:", left_width, sync_flags) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_ja_structure, SGE_NO_ABORT) >= 0) {
      if (job_is_array(job)) {
         uint32_t start, end, step;
         job_get_submit_task_ids(job, &start, &end, &step);
         std::ostringstream ss_range;
         ss_range << std::format("{}-{}:{}", start, end, step);
         os << std::format("{:<{}} {}", "job-array tasks:", left_width, ss_range.str()) << "\n";
      }
      if (uint32_t tc = lGetUlong(job, JB_ja_task_concurrency)) {
         os << std::format("{:<{}} {}", "task_concurrency:", left_width, tc) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_context, SGE_NO_ABORT) >= 0) {
      if (const lList *list = lGetList(job, JB_context)) {
         std::ostringstream ss_list;
         int fields[] = {VA_variable, VA_value, 0};
         delis[0] = "=";
         uni_print_list(ss_list, list, fields, delis, FLG_NO_DELIS_STRINGS);
         os << std::format("{:<{}} {}", "context:", left_width, ss_list.str()) << "\n";
      }
   }

   if (lGetPosViaElem(job, JB_binding, SGE_NO_ABORT) >= 0) {
      const lListElem *binding_elem = lGetObject(job, JB_binding);
      std::string binding_param;;
      ocs::BindingIo::binding_print_to_string(binding_elem, binding_param);
      os << std::format("{:<{}} {}", "binding:", left_width, binding_param) << "\n";
   }

   /* display online job usage separately for each array job but summarized over all pe_tasks */
#define SUM_UP_JATASK_USAGE(ja_task, dst, attr)                                                                        \
   if ((uep = lGetSubStr(ja_task, UA_name, attr, JAT_scaled_usage_list))) {                                            \
      (dst) += lGetDouble(uep, UA_value);                                                                                \
   }

#define SUM_UP_PETASK_USAGE(pe_task, dst, attr)                                                                        \
   if ((uep = lGetSubStr(pe_task, UA_name, attr, PET_scaled_usage))) {                                                 \
      (dst) += lGetDouble(uep, UA_value);                                                                                \
   }

   if (lGetPosViaElem(job, JB_ja_tasks, SGE_NO_ABORT) >= 0) {

      lList *ja_tasks = lGetListRW(job, JB_ja_tasks);
      if (ja_tasks != nullptr) {
         lPSortList(ja_tasks, "%I+", JAT_task_number);
      }

      const lListElem *uep;
      for_each_ep_lv(jatep, lGetList(job, JB_ja_tasks)) {
         uint32_t task_number = lGetUlong(jatep, JAT_task_number);
         // create state string and show it
         char state_string[8];
         uint32_t state = jatask_combine_state_and_status_for_output(job, jatep);
         job_get_state_string(state_string, state);
         os << std::format("{:<{}} {:>{}}: {}", "job_state", left_width_short, task_number, mid_width, state_string) << "\n";

         // show job usage information
         {
            /* jobs whose job start orders were not processed to the end
               due to a qmaster/schedd collision appear in the JB_ja_tasks
               list but are not running - thus we may not print usage for those */
            if (lGetUlong(jatep, JAT_status) == JRUNNING || lGetUlong(jatep, JAT_status) == JTRANSFERING) {
               double wallclock{}, cpu{}, mem{}, io{}, ioops{}, iow{}, vmem{}, maxvmem{}, rss{}, maxrss{};

               // In case we have execd_params ENABLE_MEM_DETAILS set, output these values as well.
               double pss{}, maxpss{}, pmem{}, smem{};
               bool have_mem_details = lGetSubStr(jatep, UA_name, USAGE_ATTR_PSS, JAT_scaled_usage_list) != nullptr;

               /* master task */
               SUM_UP_JATASK_USAGE(jatep, wallclock, USAGE_ATTR_WALLCLOCK);
               SUM_UP_JATASK_USAGE(jatep, cpu, USAGE_ATTR_CPU);
               SUM_UP_JATASK_USAGE(jatep, mem, USAGE_ATTR_MEM);
               SUM_UP_JATASK_USAGE(jatep, io, USAGE_ATTR_IO);
               SUM_UP_JATASK_USAGE(jatep, ioops, USAGE_ATTR_IOOPS);
               SUM_UP_JATASK_USAGE(jatep, iow, USAGE_ATTR_IOW);
               SUM_UP_JATASK_USAGE(jatep, vmem, USAGE_ATTR_VMEM);
               SUM_UP_JATASK_USAGE(jatep, maxvmem, USAGE_ATTR_MAXVMEM);
               SUM_UP_JATASK_USAGE(jatep, rss, USAGE_ATTR_RSS);
               SUM_UP_JATASK_USAGE(jatep, maxrss, USAGE_ATTR_MAXRSS);
               if (have_mem_details) {
                  SUM_UP_JATASK_USAGE(jatep, pss, USAGE_ATTR_PSS);
                  SUM_UP_JATASK_USAGE(jatep, maxpss, USAGE_ATTR_MAXPSS);
                  SUM_UP_JATASK_USAGE(jatep, pmem, USAGE_ATTR_PMEM);
                  SUM_UP_JATASK_USAGE(jatep, smem, USAGE_ATTR_SMEM);
               }

               /* slave tasks */
               for_each_ep_lv(pe_task_ep, lGetList(jatep, JAT_task_list)) {
                  // we do not sum up wallclock usage per pe task
                  SUM_UP_PETASK_USAGE(pe_task_ep, cpu, USAGE_ATTR_CPU);
                  SUM_UP_PETASK_USAGE(pe_task_ep, mem, USAGE_ATTR_MEM);
                  SUM_UP_PETASK_USAGE(pe_task_ep, io, USAGE_ATTR_IO);
                  SUM_UP_PETASK_USAGE(pe_task_ep, ioops, USAGE_ATTR_IOOPS);
                  SUM_UP_PETASK_USAGE(pe_task_ep, iow, USAGE_ATTR_IOW);
                  SUM_UP_PETASK_USAGE(pe_task_ep, vmem, USAGE_ATTR_VMEM);
                  SUM_UP_PETASK_USAGE(pe_task_ep, maxvmem, USAGE_ATTR_MAXVMEM);
                  SUM_UP_PETASK_USAGE(pe_task_ep, rss, USAGE_ATTR_RSS);
                  SUM_UP_PETASK_USAGE(pe_task_ep, maxrss, USAGE_ATTR_MAXRSS);
                  if (have_mem_details) {
                     SUM_UP_PETASK_USAGE(pe_task_ep, pss, USAGE_ATTR_PSS);
                     SUM_UP_PETASK_USAGE(pe_task_ep, maxpss, USAGE_ATTR_MAXPSS);
                     SUM_UP_PETASK_USAGE(pe_task_ep, pmem, USAGE_ATTR_PMEM);
                     SUM_UP_PETASK_USAGE(pe_task_ep, smem, USAGE_ATTR_SMEM);
                  }
               }

               DSTRING_STATIC(wallclock_string, 32);
               DSTRING_STATIC(cpu_string, 32);
               DSTRING_STATIC(vmem_string, 32);
               DSTRING_STATIC(maxvmem_string, 32);
               DSTRING_STATIC(rss_string, 32);
               DSTRING_STATIC(maxrss_string, 32);
               DSTRING_STATIC(iow_string, 32);

               double_print_time_to_dstring(wallclock, &wallclock_string, true);
               double_print_time_to_dstring(cpu, &cpu_string, true);
               double_print_memory_to_dstring(vmem, &vmem_string);
               double_print_memory_to_dstring(maxvmem, &maxvmem_string);
               double_print_memory_to_dstring(rss, &rss_string);
               double_print_memory_to_dstring(maxrss, &maxrss_string);
               double_print_time_to_dstring(iow, &iow_string, true);
               std::stringstream ss_usage;
               ss_usage << std::format("wallclock={},cpu={},mem={:<5.5f} GBs,io={:<5.5f},ioops={:.0f},iow={},vmem={},maxvmem={},rss={},maxrss={}",
                      sge_dstring_get_string(&wallclock_string), sge_dstring_get_string(&cpu_string),
                      mem, io, ioops, sge_dstring_get_string(&iow_string),
                      (vmem == 0.0) ? "N/A" : sge_dstring_get_string(&vmem_string),
                      (maxvmem == 0.0) ? "N/A" : sge_dstring_get_string(&maxvmem_string),
                      (rss == 0.0) ? "N/A" : sge_dstring_get_string(&rss_string),
                      (maxrss == 0.0) ? "N/A" : sge_dstring_get_string(&maxrss_string));
               if (have_mem_details) {
                  DSTRING_STATIC(pss_string, 32);
                  DSTRING_STATIC(maxpss_string, 32);
                  DSTRING_STATIC(pmem_string, 32);
                  DSTRING_STATIC(smem_string, 32);
                  double_print_memory_to_dstring(pss, &pss_string);
                  double_print_memory_to_dstring(maxpss, &maxpss_string);
                  double_print_memory_to_dstring(pmem, &pmem_string);
                  double_print_memory_to_dstring(smem, &smem_string);
                  ss_usage << std::format(",pss={},maxpss={},pmem={},smem={}",
                     sge_dstring_get_string(&pss_string), sge_dstring_get_string(&maxpss_string),
                     sge_dstring_get_string(&pmem_string), sge_dstring_get_string(&smem_string));
               }
               os << std::format("{:<{}} {:>{}}: {}", "usage", left_width_short, task_number, mid_width, ss_usage.str()) << "\n";
            }
         }

         // show binding information
         if (lGetUlong(jatep, JAT_status) == JRUNNING || lGetUlong(jatep, JAT_status) == JTRANSFERING) {
            const lList *granted_resources = lGetList(jatep, JAT_granted_resources_list);
            os << std::format("{:<{}} {:>{}}: {}", "exec_binding_list", left_width_short, task_number, mid_width, ocs::GrantedResources::to_string(granted_resources)) << "\n";
         }

         // show granted host list
         // If we do not have the list then skip the output
         if (lGetPosViaElem(jatep, JAT_granted_destin_identifier_list, SGE_NO_ABORT) >= 0) {
            delis[0] = "=";
            const lList *gdil_org = lGetList(jatep, JAT_granted_destin_identifier_list);
            std::stringstream ss_list;
            int queue_fields[] = {JG_qname, JG_slots, 0};
            uni_print_list(ss_list, gdil_org, queue_fields, delis, 0);
            os << std::format("{:<{}} {:>{}}: {}", "exec_queue_list", left_width_short, task_number, mid_width, ss_list.str());

            // get the list of granted hosts and make it unique
            // skip tasks that have not been scheduled so far
            if (lList *gdil_unique = gdil_make_host_unique(gdil_org)) {
               // print the task number and the list of granted hosts
               std::stringstream ss_list2;
               int host_fields[] = {JG_qhostname, JG_slots, 0};
               uni_print_list(ss_list2, gdil_unique, host_fields, delis, 0);
               os << std::format("{:<{}} {:>{}}: {}", "exec_host_list", left_width_short, task_number, mid_width, ss_list2.str());

               // free the list of granted hosts
               lFreeList(&gdil_unique);
            }
         }

         // show start time of each task
         DSTRING_STATIC(time_ds, 32);
         os << std::format("{:<{}} {:>{}}: {}", "start_time", left_width_short, task_number, mid_width, sge_ctime64(lGetUlong64(jatep, JAT_start_time), &time_ds)) << "\n";

         // show granted resources
         {
            bool first_task = true;
            bool is_first_remap = true;
            dstring task_resources = DSTRING_INIT;
            const char *resource_ids = nullptr;

            /* go through all RSMAP resources for the particular task and create a string */
            for_each_ep_lv (resu, lGetList(jatep, JAT_granted_resources_list)) {
               if (static_cast<ocs::GrantedResources::Type>(lGetUlong(resu, GRU_type)) == ocs::GrantedResources::Type::GRU_RESOURCE_MAP_TYPE) {
                  const char *name = lGetString(resu, GRU_name);
                  const char *host = lGetHost(resu, GRU_host);
                  const lList *ids = lGetList(resu, GRU_resource_map_list);

                  if (name != nullptr && ids != nullptr && host != nullptr) {
                     if (is_first_remap) {
                        is_first_remap = false;
                     } else {
                        sge_dstring_append(&task_resources, ",");
                     }
                     sge_dstring_append(&task_resources, name);
                     sge_dstring_append(&task_resources, "=");
                     sge_dstring_append(&task_resources, host);
                     sge_dstring_append(&task_resources, "=");
                     sge_dstring_append(&task_resources, "(");
                     std::string id_buffer;
                     sge_dstring_append(&task_resources, granted_res_get_id_string(id_buffer, ids));
                     sge_dstring_append(&task_resources, ")");
                  }
               }
            }
            /* print out granted resources */
            resource_ids = sge_dstring_get_string(&task_resources);
            os << std::format("{:<{}} {:>{}}: {}", "resource_map", left_width_short, task_number, mid_width, resource_ids != nullptr ? resource_ids : "NONE") << "\n";
            if (first_task) {
               first_task = false;
            }

            sge_dstring_free(&task_resources);
         }

         // show messages (error reasons) for each task
         for_each_ep_lv(mesobj, lGetList(jatep, JAT_message_list)) {
            if (const char *message = lGetString(mesobj, QIM_message)) {
               os << std::format("{:<{}} {:>{}}: {}", "error_reason", left_width_short, task_number, mid_width, message) << "\n";
            }
         }
      }
   }

   DRETURN_VOID;
}
