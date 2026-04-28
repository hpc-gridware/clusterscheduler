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

#include "cull/cull.h"

#include "sgeobj/ocs_BindingIo.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_usage.h"

#include "qstat/job/ocs_QStatJobViewBase.h"


void ocs::QStatJobViewBase::show_job(std::ostream &os, const lList *ilp, const lListElem *job, int flags) {
   DENTER(TOP_LAYER);

   if (job == nullptr) {
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
#if 0
   report_job_identifier_list(os, job); // What is this?
#endif
   report_script_size(os, job);
   report_script_file(os, job);
#if 0 // data is not subscribed and also not available in worker
   report_script_ptr(os, job);
#endif
   report_pe(os, job);
   report_jid_request_list(os, job);
   report_jid_predecessor_list(os, job);
   report_jid_successor_list(os, job);
   report_ja_ad_request_list(os, job);
   report_ja_ad_predecessor_list(os, job);
   report_ja_ad_successor_list(os, job);
   report_verify_suitable_queues(os, job);
#if 0 // this is data only available in execd. why should this be shown here. data also subscribed unnecessarily
   report_soft_wallclock_gmt(os, job);
   report_hard_wallclock_gmt(os, job);
#endif
   report_version(os, job);
   report_override_tickets(os, job);
   report_ar(os, job);
   report_project(os, job);
   report_department(os, job);
   report_sync_options(os, job);
   report_ja_structure(os, job);
   report_ja_task_concurrency(os, job);
   report_ctx_list(os, job);
   report_binding(os, job);

   // start task specific output
   report_task_list_started(os, job);
   if (lGetPosViaElem(job, JB_ja_tasks, SGE_NO_ABORT) >= 0) {
      if (lList *ja_tasks = lGetListRW(job, JB_ja_tasks); ja_tasks != nullptr) {
         lPSortList(ja_tasks, "%I+", JAT_task_number);
      }

      for_each_ep_lv(jatep, lGetList(job, JB_ja_tasks)) {
         report_task_started(os, job, jatep);

         report_task_id(os, job, jatep);
         report_task_state(os, job, jatep);
         report_task_usage(os, job, jatep);
         report_task_exec_binding_list(os, job, jatep);
         report_task_exec_queue_list(os, job, jatep);
         report_task_exec_host_list(os, job, jatep);
         report_task_start_time(os, job, jatep);
         report_task_resource_map(os, job, jatep);
         report_task_error_reason(os, job, jatep);

         report_task_finished(os, job, jatep);
      }
   }
   report_task_list_finished(os, job);

   // switch back to job specific output
   report_schedd_job_info(os, ilp, job);

   DRETURN_VOID;
}

void ocs::QStatJobViewBase::accumulate_usage(const lListElem *task, Usage &usage) {
   DENTER(TOP_LAYER);
   usage.have_mem_details = lGetSubStr(task, UA_name, USAGE_ATTR_PSS, JAT_scaled_usage_list) != nullptr;

   /* display online job usage separately for each array job but summarized over all pe_tasks */
#define SUM_UP_JATASK_USAGE(ja_task, dst, attr)                                                                        \
   if ((uep = lGetSubStr(ja_task, UA_name, attr, JAT_scaled_usage_list))) {                                            \
      (dst) += lGetDouble(uep, UA_value);                                                                              \
   }

#define SUM_UP_PETASK_USAGE(pe_task, dst, attr)                                                                        \
   if ((uep = lGetSubStr(pe_task, UA_name, attr, PET_scaled_usage))) {                                                 \
      (dst) += lGetDouble(uep, UA_value);                                                                              \
   }


   /* master task */
   const lListElem *uep;
   SUM_UP_JATASK_USAGE(task, usage.wallclock, USAGE_ATTR_WALLCLOCK);
   SUM_UP_JATASK_USAGE(task, usage.cpu, USAGE_ATTR_CPU);
   SUM_UP_JATASK_USAGE(task, usage.mem, USAGE_ATTR_MEM);
   SUM_UP_JATASK_USAGE(task, usage.io, USAGE_ATTR_IO);
   SUM_UP_JATASK_USAGE(task, usage.ioops, USAGE_ATTR_IOOPS);
   SUM_UP_JATASK_USAGE(task, usage.iow, USAGE_ATTR_IOW);
   SUM_UP_JATASK_USAGE(task, usage.vmem, USAGE_ATTR_VMEM);
   SUM_UP_JATASK_USAGE(task, usage.maxvmem, USAGE_ATTR_MAXVMEM);
   SUM_UP_JATASK_USAGE(task, usage.rss, USAGE_ATTR_RSS);
   SUM_UP_JATASK_USAGE(task, usage.maxrss, USAGE_ATTR_MAXRSS);
   if (usage.have_mem_details) {
      SUM_UP_JATASK_USAGE(task, usage.pss, USAGE_ATTR_PSS);
      SUM_UP_JATASK_USAGE(task, usage.maxpss, USAGE_ATTR_MAXPSS);
      SUM_UP_JATASK_USAGE(task, usage.pmem, USAGE_ATTR_PMEM);
      SUM_UP_JATASK_USAGE(task, usage.smem, USAGE_ATTR_SMEM);
   }

   /* slave tasks */
   for_each_ep_lv(pe_task_ep, lGetList(task, JAT_task_list)) {
      // we do not sum up wallclock usage per pe task
      SUM_UP_PETASK_USAGE(pe_task_ep, usage.cpu, USAGE_ATTR_CPU);
      SUM_UP_PETASK_USAGE(pe_task_ep, usage.mem, USAGE_ATTR_MEM);
      SUM_UP_PETASK_USAGE(pe_task_ep, usage.io, USAGE_ATTR_IO);
      SUM_UP_PETASK_USAGE(pe_task_ep, usage.ioops, USAGE_ATTR_IOOPS);
      SUM_UP_PETASK_USAGE(pe_task_ep, usage.iow, USAGE_ATTR_IOW);
      SUM_UP_PETASK_USAGE(pe_task_ep, usage.vmem, USAGE_ATTR_VMEM);
      SUM_UP_PETASK_USAGE(pe_task_ep, usage.maxvmem, USAGE_ATTR_MAXVMEM);
      SUM_UP_PETASK_USAGE(pe_task_ep, usage.rss, USAGE_ATTR_RSS);
      SUM_UP_PETASK_USAGE(pe_task_ep, usage.maxrss, USAGE_ATTR_MAXRSS);
      if (usage.have_mem_details) {
         SUM_UP_PETASK_USAGE(pe_task_ep, usage.pss, USAGE_ATTR_PSS);
         SUM_UP_PETASK_USAGE(pe_task_ep, usage.maxpss, USAGE_ATTR_MAXPSS);
         SUM_UP_PETASK_USAGE(pe_task_ep, usage.pmem, USAGE_ATTR_PMEM);
         SUM_UP_PETASK_USAGE(pe_task_ep, usage.smem, USAGE_ATTR_SMEM);
      }
   }
   DRETURN_VOID;
}
