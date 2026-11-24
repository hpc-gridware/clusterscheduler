/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
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

/** @file
 * @brief Controller of plain `qstat`: runs the request and drives the view
 */

#include <limits>
#include <sstream>
#include <iostream>

#include "uti/sge_bitfield.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge.h"

#include "sgeobj/ocs_BindingIo.h"
#include "sgeobj/ocs_Job.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_type.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/ocs_BindingInstance.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_BN.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_JRE.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_QIM.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_UA.h"

#include "sched/load_correction.h"
#include "sched/sge_complex_schedd.h"
#include "sched/sge_job_schedd.h"
#include "sched/sge_select_queue.h"
#include "sched/sge_urgency.h"

#include "qstat/default/ocs_QStatDefaultController.h"
#include "sched/sge_resource_utilization.h"

/** @brief Drop the tasks that were already reported with their queue
 *
 * What is left are the tasks that wait for a queue.
 *
 * @param job_list the jobs, whose tasks tagged #TAG_FOUND_IT are freed
 * @return none
 */
void ocs::QStatDefaultController::remove_tagged_jobs(const ocs::tom::List<ocs::tom::JB> job_list) {
   DENTER(TOP_LAYER);

   for (const auto jep : job_list) {
      const auto task_list = jep.get<JB_ja_tasks>();
      auto jatep = task_list.front();
      while (jatep != nullptr) {
         const auto tmp_jatep = jatep.next();
         if ((jatep.get<JAT_suitable>() & TAG_FOUND_IT)) {
            task_list.erase(jatep);
         }
         jatep = tmp_jatep;
      }
   }

   DRETURN_VOID;
}

/** @brief Report the queue instances, each followed by the jobs running in it
 *
 * Without queues in the listing only the running jobs are reported.
 *
 * @param os stream to write to
 * @param parameter the parsed parameters
 * @param model the fetched lists
 * @param view the view for the requested output format
 * @return none
 */
void
ocs::QStatDefaultController::process_queues_with_its_jobs(std::ostream &os, QStatParameter &parameter,
                                                          QStatModelBase &model, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);

   /* no need to iterate through queues if queues are not printed */
   if (!parameter.need_queues_) {
      process_jobs_in_queue(os, nullptr, true, parameter, model, view);
      DRETURN_VOID;
   }

   // handle running jobs of a queue
   for (const auto qep : model.get_qinstance_list()) {

      const char* queue_name = qep.get<QU_full_name>();

      // here we have the queue
      if (qep.get<QU_tag>() & TAG_SHOW_IT) {
         if ((parameter.show_ & QSTAT_DISPLAY_NOEMPTYQ) && !qinstance_slots_used(qep)) {
            continue;
         }

         view.report_queue_started(os, queue_name, parameter);

         process_queue(os, qep, parameter, model, view);
         process_jobs_in_queue(os, qep, true, parameter, model, view);

         view.report_queue_finished(os, queue_name, parameter);
      }
   }

   DRETURN_VOID;
}

/** @brief Report the resource requests of one scope of a job
 * @param os stream to write to
 * @param cel the requests, nothing is reported for an empty one
 * @param centry_list the complex entries, for the urgency contribution
 * @param slots the slots the job requests
 * @param scope which request scope the requests belong to: global, master or slave
 * @param is_hard_resource true for the hard requests, false for the soft ones
 * @param view the view for the requested output format
 * @return none
 */
void
ocs::QStatDefaultController::process_resources(std::ostream &os, const ocs::tom::ConstList<ocs::tom::CE> cel,
                                               const ocs::tom::ConstList<ocs::tom::CE> centry_list, const int slots,
                                               const int scope, const bool is_hard_resource,
                                               QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);

   if (cel.empty()) {
       DRETURN_VOID;
   }

   if (is_hard_resource) {
      view.report_hard_resources_started(os, scope);
   } else {
      view.report_soft_resources_started(os, scope);
   }

   /* walk through complex entries */
   for (const auto ce : cel) {
      double uc{0.0};
      const char *name = ce.get<CE_name>();
      // by name or by shortcut
      if (const auto centry = ocs::tom::ConstElement<ocs::tom::CE>::from_typed(centry_list_locate(centry_list, name));
          centry != nullptr) {
         uc = centry_urgency_contribution(slots, name, ce.get<CE_doubleval>(), centry);
      }

      const char *s = ce.get<CE_stringval>();
      if (is_hard_resource) {
         view.report_hard_resource(os, scope, ce, name, s, uc);
      } else {
         view.report_soft_resource(os, scope, ce, name, s, uc);
      }
   }
   if (is_hard_resource) {
      view.report_hard_resources_finished(os);
   } else {
      view.report_soft_resources_finished(os);
   }
   DRETURN_VOID;
}

/** @brief Report the jobs running in one queue instance, or all running jobs
 *
 * Every task found is tagged #TAG_FOUND_IT, so that it is not reported again
 * as pending. Line counts derived from the granted slots are computed without
 * signed overflow (CS-2367).
 *
 * @param os stream to write to
 * @param queue the queue instance, or an empty handle for the running jobs of all queues
 * @param print_jobs_of_queue whether the jobs are printed or only tagged
 * @param parameter the parsed parameters
 * @param model the fetched lists
 * @param view the view for the requested output format
 * @return none
 */
void
ocs::QStatDefaultController::process_jobs_in_queue(std::ostream &os, const ocs::tom::Element<ocs::tom::QU> queue,
                                                   const bool print_jobs_of_queue, QStatParameter &parameter,
                                                   QStatModelBase &model, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);
   uint32_t jid = 0, old_jid;
   uint32_t jataskid = 0, old_jataskid;
   const char *qnm = queue ? queue.get<QU_full_name>() : nullptr;
   dstring dyn_task_str = DSTRING_INIT;
   bool started = false;

   for (const auto jlep : model.get_job_list()) {
      int master, i;

      for (const auto jatep : jlep.get<JB_ja_tasks>()) {
         uint32_t jstate = jatep.get<JAT_state>();

         if (ISSET(jstate, JSUSPENDED_ON_SUBORDINATE) ||
             ISSET(jstate, JSUSPENDED_ON_SLOTWISE_SUBORDINATE)) {
            jatep.set<JAT_state>(jstate & ~JRUNNING);
         }

         ocs::tom::ConstElement<ocs::tom::JG> old_gdilep;
         for (const auto gdilep : jatep.get<JAT_granted_destin_identifier_list>()) {

            if (!queue || !strcmp(gdilep.get<JG_qname>(), qnm)) {
               int slot_adjust = 0;
               int lines_to_print;
               int slots_per_line = 0;
               const u_long raw_slots = gdilep.get<JG_slots>();
               int slots_in_queue = raw_slots > static_cast<u_long>(std::numeric_limits<int>::max())
                                    ? std::numeric_limits<int>::max()
                                    : static_cast<int>(raw_slots);

               if (!queue)
                  qnm = gdilep.get<JG_qname>();

               uint32_t job_tag = jatep.get<JAT_suitable>();
               job_tag |= TAG_FOUND_IT;
               jatep.set<JAT_suitable>(job_tag);

               master = !strcmp(qnm,
                     jatep.get<JAT_granted_destin_identifier_list>().front().get<JG_qname>());

               if (master) {
                  const char *pe_name;
                  ocs::tom::ConstElement<ocs::tom::PE> pe;
                  if (((pe_name=jatep.get<JAT_granted_pe>())) &&
                      ((pe=model.get_pe_list().find_first<PE_name>(pe_name))) &&
                      !pe.get<PE_job_is_first_task>())

                      slot_adjust = 1;
               }

               /* job distribution view ? */
               if (!(parameter.group_opt_ & GROUP_NO_PETASK_GROUPS)) {
                  /* no - condensed ouput format */
                  if (!master && !(parameter.show_ & QSTAT_DISPLAY_FULL)) {
                     /* skip all slave outputs except in full display mode */
                     continue;
                  }

                  /* print only on line per job for this queue */
                  lines_to_print = 1;

                  /* always only show the number of job slots represented by the line */
                  if ((parameter.show_ & QSTAT_DISPLAY_FULL)) {
                     slots_per_line = slots_in_queue;
                  } else {
                     slots_per_line = sge_granted_slots(jatep.get<JAT_granted_destin_identifier_list>());
                  }

               } else {
                  /* yes */
                  // saturating add: slots_in_queue is already clamped to INT_MAX,
                  // so a plain + slot_adjust could overflow (UB) (CS-2367, CWE-190)
                  lines_to_print = ProcedureView::add_saturating_int(slots_in_queue, slot_adjust);
                  slots_per_line = 1;
               }

               for (i=0; i<lines_to_print ;i++) {
                  bool print_jobid = false;
                  bool print_it = false;
                  int different;

                  old_jid = jid;
                  jid = jlep.get<JB_job_number>();
                  old_jataskid = jataskid;
                  jataskid = jatep.get<JAT_task_number>();

                  different = (jid != old_jid) || (jataskid != old_jataskid) || (gdilep != old_gdilep);
                  old_gdilep = gdilep;

                  if (different) {
                     print_jobid = true;
                  } else {
                     if (!(parameter.show_ & QSTAT_DISPLAY_RUNNING)) {
                        print_jobid = ((master && (i==0)) ? true : false);
                     } else {
                        print_jobid = false;
                     }
                  }

                  if (parameter.get_user_list().empty() ||
                     (!parameter.get_user_list().empty() && (jatep.get<JAT_suitable>()&TAG_SELECT_IT))) {
                     if (print_jobs_of_queue && (job_tag & TAG_SHOW_IT)) {
                        const uint32_t state = jatep.get<JAT_state>();
                        const uint32_t hold = jatep.get<JAT_hold>();
                        if ((parameter.show_ & QSTAT_DISPLAY_RUNNING) && (state & JRUNNING) ) {
                           print_it = true;
                        } else if ((parameter.show_ & QSTAT_DISPLAY_SUSPENDED) &&
                           ((state&JSUSPENDED) || (state&JSUSPENDED_ON_THRESHOLD) ||
                           (state&JSUSPENDED_ON_SUBORDINATE) || (state&JSUSPENDED_ON_SLOTWISE_SUBORDINATE))) {
                           print_it = true;
                        } else if ((parameter.show_ & QSTAT_DISPLAY_USERHOLD) && (hold&MINUS_H_TGT_USER)) {
                           print_it = true;
                        } else if ((parameter.show_ & QSTAT_DISPLAY_OPERATORHOLD) && (hold&MINUS_H_TGT_OPERATOR))  {
                           print_it = true;
                        } else if ((parameter.show_ & QSTAT_DISPLAY_SYSTEMHOLD) && (hold&MINUS_H_TGT_SYSTEM)) {
                           print_it = true;
                        } else if ((parameter.show_ & QSTAT_DISPLAY_JOBARRAYHOLD) && (hold&MINUS_H_TGT_JA_AD)) {
                           print_it = true;
                        } else {
                           print_it = false;
                        }
                        if (print_it) {
                           if (!started) {
                              view.report_queue_jobs_started(os, qnm, parameter);
                              started = true;
                           }

                           sge_dstring_sprintf(&dyn_task_str, sge_u32, jataskid);
                           process_job(os, jlep, jatep, queue, gdilep, print_jobid,
                                       (master && different && (i==0))?"MASTER":"SLAVE", &dyn_task_str,
                                       ProcedureView::add_saturating_int(slots_in_queue, slot_adjust), i,
                                       slots_per_line, parameter, model, view);
                        }
                     }
                  }
               }
            }
         }
      }
   }

   if (parameter.get_output_format() != ProcedureParameter::OutputFormat::JSON && !started) {
      view.report_queue_jobs_started(os, qnm, parameter);
      started = true;
   }

   if (started) {
      view.report_queue_jobs_finished(os, qnm, parameter);
   }

   sge_dstring_free(&dyn_task_str);
   DRETURN_VOID;
}

/** @brief Report one line of a job: one task, or a group of tasks in the same state
 *
 * The summary columns come first, then, as requested, the sub-tasks and the
 * additional information of `-r`.
 *
 * @param os stream to write to
 * @param job the job
 * @param jatep the task, or the first task of the group
 * @param qep the queue instance the line belongs to, an empty handle for a waiting job
 * @param gdil_ep the granted destination of the line, an empty handle for a waiting job
 * @param print_jobid whether the line starts with the job id
 * @param master `MASTER` or `SLAVE` for the job distribution view, nullptr otherwise
 * @param dyn_task_str the task id or task id range shown for an array job
 * @param slots the slots of the job in the queue instance
 * @param slot which of those slots the line stands for
 * @param slots_per_line the slots one line stands for, 0 for the slots the job requests
 * @param parameter the parsed parameters
 * @param model the fetched lists
 * @param view the view for the requested output format
 * @return none
 */
void ocs::QStatDefaultController::process_job(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                              const ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                                              const ocs::tom::ConstElement<ocs::tom::QU> qep,
                                              const ocs::tom::ConstElement<ocs::tom::JG> gdil_ep,
                                              const bool print_jobid, const char *master, dstring *dyn_task_str,
                                              const int slots, const int slot, const int slots_per_line,
                                              QStatParameter &parameter, QStatModelBase &model,
                                              QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);
   uint32_t jstate;
   ocs::tom::ConstList<ocs::tom::QR> ql;
   QStatDefaultViewBase::job_summary_t summary{};

   summary.print_jobid = print_jobid;

   if (gdil_ep) {
      summary.queue = gdil_ep.get<JG_qname>();
   }

   const bool sge_ext = (parameter.show_ & QSTAT_DISPLAY_EXTENDED) ? true : false;
   const bool tsk_ext = (parameter.show_ & QSTAT_DISPLAY_TASKS) ? true : false;
   const bool sge_urg = (parameter.show_ & QSTAT_DISPLAY_URGENCY) ? true : false;
   const bool sge_pri = (parameter.show_ & QSTAT_DISPLAY_PRIORITY) ? true : false;
   const bool sge_time = !sge_ext | tsk_ext | sge_urg | sge_pri;

   summary.nprior = jatep.get<JAT_prio>();
   if (sge_pri || sge_urg) {
      summary.nurg = job.get<JB_nurg>();
   }
   if (sge_pri) {
      summary.nppri = job.get<JB_nppri>();
   }
   if (sge_pri || sge_ext) {
      summary.ntckts =  jatep.get<JAT_ntix>();
   }
   if (sge_urg) {
      summary.urg = job.get<JB_urg>();
      summary.rrcontr = job.get<JB_rrcontr>();
      summary.wtcontr = job.get<JB_wtcontr>();
      summary.dlcontr = job.get<JB_dlcontr>();
   }
   if (sge_pri) {
      summary.priority = static_cast<int>(job.get<JB_priority>()) - BASE_PRIORITY;
   }
   summary.name = job.get<JB_job_name>();
   summary.user = job.get<JB_owner>();
   if (sge_ext) {
      summary.project = job.get<JB_project>();
      summary.department = job.get<JB_department>();
   }

   /* CS-1908: reuse the shared status-into-state combiner so the state
    * character reflects JAT_status transitions (JRUNNING, JTRANSFERING,
    * JFINISHED) plus the JAT_hold / JB_jid_predecessor / JAT_job_restarted
    * overlays. Under retention, JAT_status == JFINISHED yields
    * JFINISHED_DISPLAY -> FINISHED_SYM ('f'), matching the `-s f` filter
    * letter and distinct from the pre-retention transient 'x' (JEXITING).
    * Also picks up the JSUSPENDED handling the inlined logic was missing. */
   jstate = jatask_combine_state_and_status_for_output(job, jatep);

   job_get_state_string(summary.state, jstate);
   if (sge_time) {
      summary.submit_time = job.get<JB_submission_time>();
      summary.start_time = jatep.get<JAT_start_time>();
   }

   if (jatep.get<JAT_status>()==JRUNNING || jatep.get<JAT_status>()==JTRANSFERING) {
      summary.is_running = true;
   } else {
      summary.is_running = false;
   }

   if (sge_urg) {
      summary.deadline = job.get<JB_deadline>();
   }

   if (sge_ext) {
      ocs::tom::ConstElement<ocs::tom::UA> up;
      ocs::tom::ConstElement<ocs::tom::PE> pe;
      ocs::tom::ListPtr<ocs::tom::UA> job_usage_list;
      const char *pe_name;
      bool sum_pe_tasks = false;

      if (master == nullptr || strcmp(master, "MASTER") == 0) {
         if (!(parameter.group_opt_ & GROUP_NO_PETASK_GROUPS)) {
            sum_pe_tasks = true;
         }
         job_usage_list = jatep.get<JAT_scaled_usage_list>().clone();
      } else {
         job_usage_list = ocs::tom::ListPtr<ocs::tom::UA>::create("");
      }

      /* sum pe-task usage based on queue slots */
      if (job_usage_list) {
         int subtask_ndx=1;
         for (ocs::tom::ConstElement<ocs::tom::PET> task : jatep.get<JAT_task_list>()) {
            ocs::tom::Element<ocs::tom::UA> dst;
            ocs::tom::ConstElement<ocs::tom::JG> ep;
            const char *qname;

            if (sum_pe_tasks ||
                (summary.queue &&
                 ((ep=task.get<PET_granted_destin_identifier_list>().front())) &&
                 ((qname=ep.get<JG_qname>())) &&
                 !strcmp(qname, summary.queue) && ((subtask_ndx++%slots)==slot))) {
               for (const auto src : task.get<PET_scaled_usage>()) {
                  if ((dst=job_usage_list->find_first<UA_name>(src.get<UA_name>())))
                     dst.set<UA_value>(dst.get<UA_value>() + src.get<UA_value>());
                  else
                     job_usage_list->push_back(src.clone());
               }
            }
         }
      }


      /* scaled cpu usage */
      if (!(up = job_usage_list->find_first<UA_name>(USAGE_ATTR_CPU))) {
         summary.has_cpu_usage = false;
      } else {
         summary.has_cpu_usage = true;
         summary.cpu_usage = up.get<UA_value>();
      }
      /* scaled mem usage */
      if (!(up = job_usage_list->find_first<UA_name>(USAGE_ATTR_MEM))) {
         summary.has_mem_usage = false;
      } else {
         summary.has_mem_usage = true;
         summary.mem_usage = up.get<UA_value>();
      }
      /* scaled io usage */
      if (!(up = job_usage_list->find_first<UA_name>(USAGE_ATTR_IO))) {
         summary.has_io_usage = false;
      } else {
         summary.has_io_usage = true;
         summary.io_usage = up.get<UA_value>();
      }

      /* get tickets for job/slot */

      summary.override_tickets = job.get<JB_override_tickets>();
      summary.share = jatep.get<JAT_share>();
      if (sge_ext) {
         summary.is_queue_assigned = false;
      } else {
         if (jatep.get<JAT_granted_destin_identifier_list>() != nullptr) {
            summary.is_queue_assigned = true;
         } else {
            summary.is_queue_assigned = false;
         }
      }

      /* braces needed to suppress compiler warnings */
      if ((pe_name=jatep.get<JAT_granted_pe>()) &&
           (pe=model.get_pe_list().find_first<PE_name>(pe_name)) &&
           pe.get<PE_control_slaves>() && slots) {
         if (slot == 0) {
            summary.tickets = (u_long)gdil_ep.get<JG_ticket>();
            summary.otickets = (u_long)gdil_ep.get<JG_oticket>();
            summary.ftickets = (u_long)gdil_ep.get<JG_fticket>();
            summary.stickets = (u_long)gdil_ep.get<JG_sticket>();
         }
         else {
            if (slots) {
               summary.tickets = (u_long)(gdil_ep.get<JG_ticket>() / slots);
               summary.otickets = (u_long)(gdil_ep.get<JG_oticket>() / slots);
               summary.ftickets = (u_long)(gdil_ep.get<JG_fticket>() / slots);
               summary.stickets = (u_long)(gdil_ep.get<JG_sticket>() / slots);
            }
            else {
               summary.tickets = summary.otickets = summary.ftickets = summary.stickets = 0;
            }
         }
      }
      else {
         summary.tickets = (u_long)jatep.get<JAT_tix>();
         summary.otickets = (u_long)jatep.get<JAT_oticket>();
         summary.ftickets = (u_long)jatep.get<JAT_fticket>();
         summary.stickets = (u_long)jatep.get<JAT_sticket>();
      }

   }

   summary.master = master;
   if (slots_per_line == 0) {
      summary.slots = sge_job_slot_request(job, model.get_pe_list());
   } else {
      summary.slots = slots_per_line;
   }

   summary.is_array = job_is_array(job);

   if (summary.is_array) {
      summary.task_id = sge_dstring_get_string(dyn_task_str);
   } else {
      summary.task_id = nullptr;
   }

   view.report_job(os, job.get<JB_job_number>(), &summary, parameter, model);

   if (tsk_ext) {
      ocs::tom::ConstList<ocs::tom::PET> task_list = jatep.get<JAT_task_list>();
      ocs::tom::ConstElement<ocs::tom::JG> ep;
      const char *qname;
      int subtask_ndx=1;
      bool close_tasks{false};

      /* print master sub-task belonging to this queue */
      if (!slot && task_list && summary.queue &&
          ((ep=jatep.get<JAT_granted_destin_identifier_list>().front())) &&
          ((qname=ep.get<JG_qname>())) &&
          !strcmp(qname, summary.queue)) {

         if (!close_tasks) {
            view.report_sub_tasks_started(os);
            close_tasks = true;
         }

         process_subtask(os, job, jatep, nullptr, view);
      }

      /* print sub-tasks belonging to this queue */
      for (const auto task : task_list) {
         if (!slots || (summary.queue &&
              ((ep=task.get<PET_granted_destin_identifier_list>().front())) &&
              ((qname=ep.get<JG_qname>())) &&
              !strcmp(qname, summary.queue) && ((subtask_ndx++%slots)==slot))) {
            if (!close_tasks) {
               view.report_sub_tasks_started(os);
               close_tasks = true;
            }
            process_subtask(os, job, jatep, task, view);
         }
      }

      if (close_tasks) {
         view.report_sub_tasks_finished(os);
      }
   }

   /* print additional job info if requested */
   if ((parameter.show_ & QSTAT_DISPLAY_RESOURCES)) {

      view.report_additional_info(os, QStatDefaultViewBase::FULL_JOB_NAME, job.get<JB_job_name>());
      if (summary.queue) {
         view.report_additional_info(os, QStatDefaultViewBase::MASTER_QUEUE, summary.queue);
      }

      if (job.get<JB_pe>()) {
         dstring range_string = DSTRING_INIT;

         range_list_print_to_string(job.get<JB_pe_range>(),
                                    &range_string, true, false, false);

         view.report_requested_pe(os, job.get<JB_pe>(), sge_dstring_get_string(&range_string));

         sge_dstring_free(&range_string);
      }

      if (jatep.get<JAT_granted_pe>()) {
         uint32_t pe_slots = 0;
         for (const auto l_gdil_ep : jatep.get<JAT_granted_destin_identifier_list>()) {
            pe_slots += l_gdil_ep.get<JG_slots>();
         }

         view.report_granted_pe(os, jatep.get<JAT_granted_pe>(), pe_slots);
      }
      if (job.get<JB_checkpoint_name>()) {
         view.report_additional_info(os, QStatDefaultViewBase::CHECKPOINT_ENV, job.get<JB_checkpoint_name>());
      }

      /* Handle the Hard Resources (global, master, slave) */
      for (const uint32_t scope : {JRS_SCOPE_GLOBAL, JRS_SCOPE_MASTER, JRS_SCOPE_SLAVE}) {
         const auto hard_resources =
            ocs::tom::ConstList<ocs::tom::CE>::from_typed(job_get_hard_resource_list(job, scope));
         process_resources(os, hard_resources, model.get_centry_list(), sge_job_slot_request(job, model.get_pe_list()),
                           static_cast<int>(scope), true, view);
      }

      /* display default requests if necessary */
      {
         const char *name;
         ocs::tom::ConstElement<ocs::tom::EH> hep;
         bool first_default_request{true};

         const auto attributes = queue_complexes2scheduler(qep, model.get_exechost_list(), model.get_centry_list(),
                                                           model.get_job_load_adjustments());
         for (const auto ce : *attributes) {
            name = ce.get<CE_name>();
            if (!ce.get<CE_consumable>() || !strcmp(name, "slots") ||
                job_get_request(job, name)) {
               continue;
            }

            // CS-459: parsed once by qmaster, 0 if the default does not parse
            if (ce.get<CE_default_doubleval>() == 0.0) {
               continue;
            }

            /* For pending jobs (no queue/no exec host) we may print default request only
               if the consumable is specified in the global host. For running we print it
               if the resource is managed at this node/queue */
            if ((qep && qep.get<QU_consumable_config_list>().find_first<CE_name>(name)) ||
                (qep && (hep=ocs::tom::ConstElement<ocs::tom::EH>::from_typed(
                            host_list_locate(model.get_exechost_list(), qep.get<QU_qhostname>()))) &&
                 hep.get<EH_consumable_config_list>().find_first<CE_name>(name)) ||
                  ((hep=ocs::tom::ConstElement<ocs::tom::EH>::from_typed(
                       host_list_locate(model.get_exechost_list(), SGE_GLOBAL_NAME))) &&
                  hep.get<EH_consumable_config_list>().find_first<CE_name>(name))) {
               if (first_default_request) {
                  view.report_default_request_started(os);
                  first_default_request = false;
               }
               view.report_default_request(os, name, ce.get<CE_defaultval>());
            }
         }
         if (!first_default_request) {
            view.report_default_request_finished(os);
         }
      }

      /* Handle the Soft Resources */
      process_resources(os, ocs::tom::ConstList<ocs::tom::CE>::from_typed(job_get_soft_resource_list(job)),
                        model.get_centry_list(), sge_job_slot_request(job, model.get_pe_list()), JRS_SCOPE_GLOBAL,
                        false, view);

      {
         ql = ocs::tom::ConstList<ocs::tom::QR>::from_typed(job_get_hard_queue_list(job, JRS_SCOPE_GLOBAL));
         if (!ql.empty()) {
            view.report_hard_requested_queues_started(os, JRS_SCOPE_GLOBAL);
            for (const auto qrep : ql) {
               view.report_hard_requested_queue(os, JRS_SCOPE_GLOBAL, qrep.get<QR_name>());
            }
            view.report_hard_requested_queues_finished(os);
         }
         ql = ocs::tom::ConstList<ocs::tom::QR>::from_typed(job_get_hard_queue_list(job, JRS_SCOPE_MASTER));
         if (!ql.empty()) {
            view.report_hard_requested_queues_started(os, JRS_SCOPE_MASTER);
            for (const auto qrep : ql) {
               view.report_hard_requested_queue(os, JRS_SCOPE_MASTER, qrep.get<QR_name>());
            }
            view.report_hard_requested_queues_finished(os);
         }
         ql = ocs::tom::ConstList<ocs::tom::QR>::from_typed(job_get_hard_queue_list(job, JRS_SCOPE_SLAVE));
         if (!ql.empty()) {
            view.report_hard_requested_queues_started(os, JRS_SCOPE_SLAVE);
            for (const auto qrep : ql) {
               view.report_hard_requested_queue(os, JRS_SCOPE_SLAVE, qrep.get<QR_name>());
            }
            view.report_hard_requested_queues_finished(os);
         }
      }

      {
         ql = ocs::tom::ConstList<ocs::tom::QR>::from_typed(job_get_soft_queue_list(job, JRS_SCOPE_GLOBAL));
         if (!ql.empty()) {
            view.report_soft_requested_queues_started(os,JRS_SCOPE_GLOBAL);
            for (const auto qrep : ql) {
               view.report_soft_requested_queue(os, JRS_SCOPE_GLOBAL, qrep.get<QR_name>());
            }
            view.report_soft_requested_queues_finished(os);
         }
         ql = ocs::tom::ConstList<ocs::tom::QR>::from_typed(job_get_soft_queue_list(job, JRS_SCOPE_MASTER));
         if (!ql.empty()) {
            view.report_soft_requested_queues_started(os, JRS_SCOPE_MASTER);
            for (const auto qrep : ql) {
               view.report_soft_requested_queue(os, JRS_SCOPE_MASTER, qrep.get<QR_name>());
            }
            view.report_soft_requested_queues_finished(os);
         }
         ql = ocs::tom::ConstList<ocs::tom::QR>::from_typed(job_get_soft_queue_list(job, JRS_SCOPE_SLAVE));
         if (!ql.empty()) {
            view.report_soft_requested_queues_started(os, JRS_SCOPE_SLAVE);
            for (const auto qrep : ql) {
               view.report_soft_requested_queue(os, JRS_SCOPE_SLAVE, qrep.get<QR_name>());
            }
            view.report_soft_requested_queues_finished(os);
         }
      }

      {
         ocs::tom::ConstList<ocs::tom::JRE> ql = job.get<JB_jid_request_list>();
         if (ql) {
            view.report_predecessors_requested_started(os);
            for (ocs::tom::ConstElement<ocs::tom::JRE> qrep : ql) {
               view.report_predecessor_requested(os, qrep.get<JRE_job_name>());
            }
            view.report_predecessors_requested_finished(os);
         }
      }
      {
         ocs::tom::ConstList<ocs::tom::JRE> ql = job.get<JB_jid_predecessor_list>();
         if (ql) {
            view.report_predecessors_started(os);
            for (ocs::tom::ConstElement<ocs::tom::JRE> qrep : ql) {
               view.report_predecessor(os, qrep.get<JRE_job_number>());
            }
            view.report_predecessors_finished(os);
         }
      }

      {
         ocs::tom::ConstList<ocs::tom::JRE> ql = job.get<JB_ja_ad_request_list>();
         if (ql) {
            view.report_ad_predecessors_requested_started(os);
            for (ocs::tom::ConstElement<ocs::tom::JRE> qrep : ql) {
               view.report_ad_predecessor_requested(os, qrep.get<JRE_job_name>());
            }
            view.report_ad_predecessors_requested_finished(os);
         }
      }
      {
         ocs::tom::ConstList<ocs::tom::JRE> ql = job.get<JB_ja_ad_predecessor_list>();
         if (ql) {
            view.report_ad_predecessors_started(os);
            for (ocs::tom::ConstElement<ocs::tom::JRE> qrep : ql) {
               view.report_ad_predecessor(os, qrep.get<JRE_job_number>());
            }
            view.report_ad_predecessors_finished(os);
         }
      }

      if (ocs::tom::ConstElement<ocs::tom::BN> binding_elem = job.get<JB_binding>(); binding_elem != nullptr) {
         dstring binding_param = DSTRING_INIT;

         // all binding specific parameter as one string
         std::string binding_str;
         BindingIo::binding_print_to_string(binding_elem, binding_str);
         sge_dstring_sprintf(&binding_param, "%s", binding_str.c_str());

         view.report_binding_started(os);
         view.report_binding(os, sge_dstring_get_string(&binding_param));
         view.report_binding_attribute(os, "bamount", binding_elem.get<BN_amount>());
         view.report_binding_attribute(os, "binstance", BindingInstance::to_string(
            static_cast<BindingInstance::Instance>(binding_elem.get<BN_instance>())).c_str());
         view.report_binding_attribute(os, "bstrategy", BindingStrategy::to_string(
            static_cast<BindingStrategy::Strategy>(binding_elem.get<BN_strategy>())).c_str());
         view.report_binding_attribute(os, "btype", BindingType::to_string(
            static_cast<BindingType::Type>(binding_elem.get<BN_new_type>())).c_str());
         view.report_binding_attribute(os, "bunit", BindingUnit::to_string(
            static_cast<BindingUnit::Unit>(binding_elem.get<BN_unit>())).c_str());
         view.report_binding_attribute(os, "bfilter", binding_elem.get<BN_filter>());
         view.report_binding_attribute(os, "bsort", binding_elem.get<BN_sort>());
         view.report_binding_attribute(os, "bstart", BindingStart::to_string(
            static_cast<BindingStart::Start>(binding_elem.get<BN_start>())).c_str());
         view.report_binding_attribute(os, "bstop", BindingStop::to_string(
            static_cast<BindingStop::Stop>(binding_elem.get<BN_stop>())).c_str());
         view.report_binding_finished(os);
         sge_dstring_free(&binding_param);
      }
   }

   view.report_job_finished(os, job.get<JB_job_number>());

   DRETURN_VOID;
}

/** @brief Report one sub-task of a parallel job: the master task or one PE task
 * @param os stream to write to
 * @param job the job
 * @param ja_task the array task
 * @param pe_task the PE task, or an empty handle for the master task
 * @param view the view for the requested output format
 * @return none
 */
void
ocs::QStatDefaultController::process_subtask(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                                             const ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                                             const ocs::tom::ConstElement<ocs::tom::PET> pe_task,
                                             QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);
   char task_state_string[8];
   uint32_t tstate, tstatus;
   ocs::tom::ConstList<ocs::tom::UA> usage_list;
   ocs::tom::ConstList<ocs::tom::UA> scaled_usage_list;

   QStatDefaultViewBase::task_summary_t summary{};

   /* is sub-task logically running */
   if (pe_task == nullptr) {
      tstatus = ja_task.get<JAT_status>();
      usage_list = ja_task.get<JAT_usage_list>();
      scaled_usage_list = ja_task.get<JAT_scaled_usage_list>();
   } else {
      tstatus = pe_task.get<PET_status>();
      usage_list = pe_task.get<PET_usage>();
      scaled_usage_list = pe_task.get<PET_scaled_usage>();
   }

   if (pe_task == nullptr) {
      summary.task_id = "";
   } else {
      summary.task_id = pe_task.get<PET_id>();
   }

   /* move status info into state info */
   tstate = ja_task.get<JAT_state>();
   if (tstatus==JRUNNING) {
      tstate |= JRUNNING;
      tstate &= ~JTRANSFERING;
   } else if (tstatus==JTRANSFERING) {
      tstate |= JTRANSFERING;
      tstate &= ~JRUNNING;
   } else if (tstatus==JFINISHED) {
      /* CS-1908: retained finished ja_task rows render as 'f'
       * (FINISHED_SYM via JFINISHED_DISPLAY) rather than the pre-retention
       * transient 'x' (EXITING_SYM via JEXITING). See the parallel change
       * in jatask_combine_state_and_status_for_output. */
      tstate |= JFINISHED_DISPLAY;
      tstate &= ~(JRUNNING|JTRANSFERING|JEXITING);
   }

   if (job.get<JB_jid_predecessor_list>() || ja_task.get<JAT_hold>()) {
      tstate |= JHELD;
   }

   if (ja_task.get<JAT_job_restarted>()) {
      tstate &= ~JWAITING;
      tstate |= JMIGRATING;
   }

   /* write states into string */
   job_get_state_string(task_state_string, tstate);
   summary.state = task_state_string;

   {
      ocs::tom::ConstElement<ocs::tom::UA> up;

      /* scaled cpu usage */
      if (!(up = scaled_usage_list.find_first<UA_name>(USAGE_ATTR_CPU))) {
         summary.has_cpu_usage = false;
      } else {
         summary.has_cpu_usage = true;
         summary.cpu_usage = up.get<UA_value>();
      }

      /* scaled mem usage */
      if (!(up = scaled_usage_list.find_first<UA_name>(USAGE_ATTR_MEM))) {
         summary.has_mem_usage = false;
      } else {
         summary.has_mem_usage = true;
         summary.mem_usage = up.get<UA_value>();
      }

      /* scaled io usage */
      if (!(up = scaled_usage_list.find_first<UA_name>(USAGE_ATTR_IO))) {
         summary.has_io_usage = false;
      } else {
         summary.has_io_usage = true;
         summary.io_usage = up.get<UA_value>();
      }
   }

   if (tstatus==JFINISHED) {
      ocs::tom::ConstElement<ocs::tom::UA> ep=usage_list.find_first<UA_name>("exit_status");
      if (ep) {
         summary.has_exit_status = true;
         summary.exit_status = ep.get<UA_value>();
      } else {
         summary.has_exit_status = false;
      }
   } else {
      summary.has_exit_status = false;
   }

   view.report_sub_task(os, &summary);

   DRETURN_VOID;
}

/** @brief Report the waiting tasks of all jobs
 *
 * Tasks in the same state are grouped into one line with a task id range,
 * unless `-g d` asks for one line per task. The tasks not yet enrolled
 * follow per job.
 *
 * @param os stream to write to
 * @param parameter the parsed parameters
 * @param model the fetched lists
 * @param view the view for the requested output format
 * @return none
 */
void ocs::QStatDefaultController::process_jobs_pending_state(std::ostream &os, QStatParameter &parameter,
                                                             QStatModelBase &model, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::JAT> ja_task_list;
   int FoundTasks;
   int count = 0;
   dstring dyn_task_str = DSTRING_INIT;

   auto nxt = model.get_job_list().front();
   while (nxt != nullptr) {
      const auto jep = nxt;
      nxt = jep.next();
      auto nxt_jatep = jep.get<JB_ja_tasks>().front();
      FoundTasks = 0;

      bool hide_data = !job_is_visible(parameter.get_packet(), jep.get<JB_owner>(), model.is_manager());
      if (hide_data) {
         continue;
      }

      while (nxt_jatep != nullptr) {
         const auto jatep = nxt_jatep;
         nxt_jatep = jatep.next();

         if (!(((parameter.show_ & QSTAT_DISPLAY_OPERATORHOLD) && (jatep.get<JAT_hold>()&MINUS_H_TGT_OPERATOR))
               ||
             ((parameter.show_ & QSTAT_DISPLAY_USERHOLD) && (jatep.get<JAT_hold>()&MINUS_H_TGT_USER))
               ||
             ((parameter.show_ & QSTAT_DISPLAY_SYSTEMHOLD) && (jatep.get<JAT_hold>()&MINUS_H_TGT_SYSTEM))
               ||
             ((parameter.show_ & QSTAT_DISPLAY_JOBARRAYHOLD) && (jatep.get<JAT_hold>()&MINUS_H_TGT_JA_AD))
               ||
             ((parameter.show_ & QSTAT_DISPLAY_JOBHOLD) && jep.get<JB_jid_predecessor_list>())
               ||
             ((parameter.show_ & QSTAT_DISPLAY_STARTTIMEHOLD) && jep.get<JB_execution_time>())
               ||
             !(parameter.show_ & QSTAT_DISPLAY_HOLD))
            ) {
            break;
         }

         if (!(jatep.get<JAT_suitable>() & TAG_FOUND_IT) &&
            VALID(JQUEUED, jatep.get<JAT_state>()) &&
            !VALID(JFINISHED, jatep.get<JAT_status>())) {
            jatep.set<JAT_suitable>(
            jatep.get<JAT_suitable>()|TAG_FOUND_IT);

            if ((parameter.get_user_list().empty() ||
                 (!parameter.get_user_list().empty() && (jatep.get<JAT_suitable>()&TAG_SELECT_IT))) &&
                (jatep.get<JAT_suitable>()&TAG_SHOW_IT)) {

               if ((parameter.show_ & QSTAT_DISPLAY_PENDING) &&
                   (parameter.group_opt_ & GROUP_NO_TASK_GROUPS) > 0) {

                  sge_dstring_sprintf(&dyn_task_str, sge_u32, jatep.get<JAT_task_number>());

                  if (count == 0) {
                     view.report_pending_jobs_started(os, parameter);
                  }
                  process_job(os, jep, jatep, nullptr, nullptr, true, nullptr, &dyn_task_str, 0, 0, 0, parameter,
                              model, view);

                  count++;
               } else {
                  if (!ja_task_list) {
                     // the copies keep the reduced layout the tasks were fetched with
                     ja_task_list = ocs::tom::ListPtr<ocs::tom::JAT>::create_like("", jatep);
                  }
                  ja_task_list->push_back(jatep.clone());
                  FoundTasks = 1;
               }
            }
         }
      }
      if ((parameter.show_ & QSTAT_DISPLAY_PENDING)  &&
          (parameter.group_opt_ & GROUP_NO_TASK_GROUPS) == 0 &&
          FoundTasks &&
          ja_task_list) {
         while (const auto task_group = ja_task_list_split_group(ja_task_list)) {
            sge_dstring_clear(&dyn_task_str);
            ja_task_list_print_to_string(*task_group, &dyn_task_str);

            if (count == 0) {
               view.report_pending_jobs_started(os, parameter);
            }
            process_job(os, jep, task_group->front(), nullptr, nullptr, true, nullptr, &dyn_task_str, 0, 0, 0,
                        parameter, model, view);

            count++;
         }
      }
      if (jep != nxt && (parameter.show_ & QSTAT_DISPLAY_PENDING)) {
         process_jobs_not_enrolled(os, jep, true, nullptr, 0, 0, &count, parameter, model, view);
      }
   }

   if (count > 0) {
      view.report_pending_jobs_finished(os);
   }

   sge_dstring_free(&dyn_task_str);
   DRETURN_VOID;
}

/** @brief Report the finished tasks the retention window still holds
 *
 * Only with `-s f` or an all-states filter. Tasks are grouped as in
 * process_jobs_pending_state().
 *
 * @param os stream to write to
 * @param parameter the parsed parameters
 * @param model the fetched lists
 * @param view the view for the requested output format
 * @return none
 */
void ocs::QStatDefaultController::process_jobs_finished_state(std::ostream &os, QStatParameter &parameter,
                                                              QStatModelBase &model, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);
   int count = 0;
   dstring dyn_task_str = DSTRING_INIT;

   /* CS-1908 U7: render finished ja_tasks only when the caller opted in via
    * qstat -s f (QSTAT_DISPLAY_FINISHED bit) or an all-states filter. Prior
    * to CS-1908 this branch was gated behind the MORE_INFO env-var debug
    * hook. */
   if ((parameter.show_ & QSTAT_DISPLAY_FINISHED) == 0) {
      sge_dstring_free(&dyn_task_str);
      DRETURN_VOID;
   }

   for (const auto jep : model.get_job_list()) {
      ocs::tom::ListPtr<ocs::tom::JAT> ja_task_list;
      bool FoundTasks = false;

      for (const auto jatep : jep.get<JB_ja_tasks>()) {
         if (jatep.get<JAT_status>() != JFINISHED) {
            continue;
         }
         jatep.set<JAT_suitable>(jatep.get<JAT_suitable>() | TAG_FOUND_IT);

         if (!parameter.get_user_list().empty() &&
             !(jatep.get<JAT_suitable>() & TAG_SELECT_IT)) {
            continue;
         }

         /* CS-1908 U7: -g d drills down to per-JAT rows; otherwise range-group
          * the finished JATs of this JB into one summary row (state character
          * `f`, JAT-id-range in the ja-task-ID column). Mirrors the pending
          * path's summary-vs-detail fork. */
         if ((parameter.group_opt_ & GROUP_NO_TASK_GROUPS) > 0) {
            sge_dstring_sprintf(&dyn_task_str, sge_u32, jatep.get<JAT_task_number>());

            if (count == 0) {
               view.report_finished_jobs_started(os, parameter);
            }
            process_job(os, jep, jatep, nullptr, nullptr, true, nullptr, &dyn_task_str, 0, 0, 0, parameter, model,
                        view);

            count++;
         } else {
            if (ja_task_list == nullptr) {
               // the copies keep the reduced layout the tasks were fetched with
               ja_task_list = ocs::tom::ListPtr<ocs::tom::JAT>::create_like("", jatep);
            }
            ja_task_list->push_back(jatep.clone());
            FoundTasks = true;
         }
      }

      if ((parameter.group_opt_ & GROUP_NO_TASK_GROUPS) == 0 && FoundTasks && ja_task_list != nullptr) {
         while (const auto task_group = ja_task_list_split_group(ja_task_list)) {
            sge_dstring_clear(&dyn_task_str);
            ja_task_list_print_to_string(*task_group, &dyn_task_str);

            if (count == 0) {
               view.report_finished_jobs_started(os, parameter);
            }
            process_job(os, jep, task_group->front(), nullptr, nullptr, true, nullptr, &dyn_task_str, 0, 0, 0,
                        parameter, model, view);

            count++;
         }
      }
   }

   if (count > 0) {
      view.report_finished_jobs_finished(os);
   }

   sge_dstring_free(&dyn_task_str);
   DRETURN_VOID;
}

/** @brief Report the tasks in error state that were not reported yet
 * @param os stream to write to
 * @param parameter the parsed parameters
 * @param model the fetched lists
 * @param view the view for the requested output format
 * @return none
 */
void ocs::QStatDefaultController::process_jobs_error_state(std::ostream &os, QStatParameter &parameter,
                                                           QStatModelBase &model, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);
   int count = 0;
   dstring dyn_task_str = DSTRING_INIT;

   for (const auto jep : model.get_job_list()) {
      for (const auto jatep : jep.get<JB_ja_tasks>()) {
         if (!(jatep.get<JAT_suitable>() & TAG_FOUND_IT) && jatep.get<JAT_status>() == JERROR) {
            jatep.set<JAT_suitable>(jatep.get<JAT_suitable>()|TAG_FOUND_IT);

            if (parameter.get_user_list().empty() ||
                (!parameter.get_user_list().empty() && (jatep.get<JAT_suitable>()&TAG_SELECT_IT))) {
               sge_dstring_sprintf(&dyn_task_str, sge_u32, jatep.get<JAT_task_number>());

               if (count == 0) {
                   view.report_error_jobs_started(os, parameter);
               }
               process_job(os, jep, jatep, nullptr, nullptr, true, nullptr, &dyn_task_str, 0, 0, 0, parameter, model,
                           view);

               count++;
            }
         }
      }
   }
   if (count > 0 ) {
       view.report_error_jobs_finished(os);
   }

   sge_dstring_free(&dyn_task_str);
   DRETURN_VOID;
}

/** @brief Report the tasks of a job that are not enrolled yet, by hold state
 *
 * Tasks with the same hold state are one line with a task id range, unless
 * `-g d` asks for one line per task.
 *
 * @param os stream to write to
 * @param job the job; its id lists of held tasks are taken out while a line is reported and put back after
 * @param print_jobid whether the lines start with the job id
 * @param master `MASTER` or `SLAVE` for the job distribution view, nullptr otherwise
 * @param slots the slots of the job in the queue instance, 0 for a waiting job
 * @param slot which of those slots the line stands for
 * @param count the number of lines reported in the section so far, increased by the lines reported here
 * @param parameter the parsed parameters
 * @param model the fetched lists
 * @param view the view for the requested output format
 * @return none
 */
void ocs::QStatDefaultController::process_jobs_not_enrolled(std::ostream &os, const ocs::tom::Element<ocs::tom::JB> job,
                                                            const bool print_jobid, char *master, const int slots,
                                                            const int slot, int *count, QStatParameter &parameter,
                                                            QStatModelBase &model, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);
   dstring ja_task_id_string = DSTRING_INIT;

   const auto hold_id_lists = job_create_hold_id_lists(job);
   for (const auto &[hold_state, range_list] : hold_id_lists) {
      ocs::tom::ListPtr<ocs::tom::AN> answer_list;
      uint32_t first_id;
      int show = 0;

      if (((parameter.show_ & QSTAT_DISPLAY_USERHOLD) && (hold_state & MINUS_H_TGT_USER)) ||
          ((parameter.show_ & QSTAT_DISPLAY_OPERATORHOLD) && (hold_state & MINUS_H_TGT_OPERATOR)) ||
          ((parameter.show_ & QSTAT_DISPLAY_SYSTEMHOLD) && (hold_state & MINUS_H_TGT_SYSTEM)) ||
          ((parameter.show_ & QSTAT_DISPLAY_JOBARRAYHOLD) && (hold_state & MINUS_H_TGT_JA_AD)) ||
          ((parameter.show_ & QSTAT_DISPLAY_STARTTIMEHOLD) && (job.get<JB_execution_time>() > 0)) ||
          ((parameter.show_ & QSTAT_DISPLAY_JOBHOLD) && (job.get<JB_jid_predecessor_list>() != nullptr)) ||
          (!(parameter.show_ & QSTAT_DISPLAY_HOLD))
         ) {
         show = 1;
      }
      if (range_list && show) {
         if ((parameter.group_opt_ & GROUP_NO_TASK_GROUPS) == 0) {
            sge_dstring_clear(&ja_task_id_string);
            range_list_print_to_string(*range_list, &ja_task_id_string, false, false, false);
            first_id = range_list_get_first_id(*range_list, &answer_list);
            if (answer_list_has_error(&answer_list) != 1) {
               const auto ja_task = ocs::tom::ConstElement<ocs::tom::JAT>::from_typed(
                  job_get_ja_task_template_hold(job, first_id, hold_state));
               // the id lists are taken out while the line is reported and put back after
               auto n_h_ids = job.exchange<JB_ja_n_h_ids>({});
               auto u_h_ids = job.exchange<JB_ja_u_h_ids>({});
               auto o_h_ids = job.exchange<JB_ja_o_h_ids>({});
               auto s_h_ids = job.exchange<JB_ja_s_h_ids>({});
               auto a_h_ids = job.exchange<JB_ja_a_h_ids>({});

               if (*count == 0) {
                  view.report_pending_jobs_started(os, parameter);
               }
               process_job(os, job, ja_task, nullptr, nullptr, print_jobid, master, &ja_task_id_string, slots, slot,
                           0, parameter, model, view);
               job.exchange<JB_ja_n_h_ids>(std::move(n_h_ids));
               job.exchange<JB_ja_u_h_ids>(std::move(u_h_ids));
               job.exchange<JB_ja_o_h_ids>(std::move(o_h_ids));
               job.exchange<JB_ja_s_h_ids>(std::move(s_h_ids));
               job.exchange<JB_ja_a_h_ids>(std::move(a_h_ids));
               (*count)++;
            }
         } else {
            for (const auto range : *range_list) {
               uint32_t start, end, step;
               range_get_all_ids(range, &start, &end, &step);
               for (; start <= end; start += step) {
                  const auto ja_task = ocs::tom::ConstElement<ocs::tom::JAT>::from_typed(
                     job_get_ja_task_template_hold(job, start, hold_state));
                  sge_dstring_sprintf(&ja_task_id_string, sge_u32, start);

                  if (*count == 0) {
                     view.report_pending_jobs_started(os, parameter);
                  }
                  process_job(os, job, ja_task, nullptr, nullptr, print_jobid, nullptr, &ja_task_id_string, slots,
                              slot, 0, parameter, model, view);
                  (*count)++;
               }
            }
         }
      }
      answer_list.reset();
   }

   sge_dstring_free(&ja_task_id_string);
   DRETURN_VOID;
}

/** @brief Report the summary line of one queue instance and, as requested, why it is in alarm and its resources
 *
 * Nothing is reported without `-f`. The load and suspend alarm states of the
 * instance are set on the way.
 *
 * @param os stream to write to
 * @param queue the queue instance
 * @param parameter the parsed parameters
 * @param model the fetched lists
 * @param view the view for the requested output format
 * @return none
 */
void
ocs::QStatDefaultController::process_queue(std::ostream &os, const ocs::tom::Element<ocs::tom::QU> queue,
                                           QStatParameter &parameter, QStatModelBase &model,
                                           QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);

   if (!(parameter.show_ & QSTAT_DISPLAY_FULL)) {
      DRETURN_VOID;
   }

   QStatDefaultViewBase::queue_summary_t summary {};
   // These fields must own their storage: they used to be const char* aliasing
   // stack locals below (a latent use-after-scope). Pin that a revert to a
   // non-owning type fails to compile (CS-2366, LOW-QSTAT-003).
   static_assert(std::is_same_v<decltype(summary.queue_type), std::string> &&
                 std::is_same_v<decltype(summary.arch), std::string> &&
                 std::is_same_v<decltype(summary.state), std::string>,
                 "queue_summary_t queue_type/arch/state must own their storage");

   // CS-2387: load variable resolved on the client from SGE_QSTAT_LOAD_AVG and
   // marshalled through QStatParameter, so both client- and server-rendered
   // qstat (see ExecContext::SERVER path in ocs_qstat.cc) honour the caller's env.
   summary.load_avg_str = parameter.get_load_avg_variable();

   // compute the load and check for alarm states
   summary.has_load_value = sge_get_double_qattr(&summary.load_avg, summary.load_avg_str, queue,
                                                 model.get_exechost_list(), model.get_centry_list(),
                                                 model.get_job_load_adjustments(),
                                                 &summary.has_load_value_from_object) ? true : false;

   char load_alarm_reason[MAX_STRING_SIZE] {};
   if (sge_load_alarm(nullptr, 0, queue, queue.get<QU_load_thresholds>(), model.get_exechost_list(),
                      model.get_centry_list(), ocs::tom::ConstList<ocs::tom::CE>(), model.get_job_load_adjustments(),
                      true)) {
      qinstance_state_set_alarm(queue, true);

      sge_load_alarm_reason(queue, queue.get<QU_load_thresholds>(), model.get_exechost_list(),
                            model.get_centry_list(), model.get_job_load_adjustments(), load_alarm_reason,
                            MAX_STRING_SIZE - 1, "load");
   }

   char suspend_alarm_reason[MAX_STRING_SIZE] {};
   uint32_t interval;
   parse_ulong_val(nullptr, &interval, CEntry::Type::TIME, queue.get<QU_suspend_interval>(), nullptr, 0);
   if (queue.get<QU_nsuspend>() != 0 && interval != 0 &&
       sge_load_alarm(nullptr, 0, queue, queue.get<QU_suspend_thresholds>(), model.get_exechost_list(),
                      model.get_centry_list(), ocs::tom::ConstList<ocs::tom::CE>(), model.get_job_load_adjustments(),
                      false)) {
      qinstance_state_set_suspend_alarm(queue, true);
      sge_load_alarm_reason(queue, queue.get<QU_suspend_thresholds>(), model.get_exechost_list(),
                            model.get_centry_list(), model.get_job_load_adjustments(), suspend_alarm_reason,
                            MAX_STRING_SIZE - 1, "suspend");
   }

   DSTRING_STATIC(type_string, 32);
   qinstance_print_qtype_to_dstring(queue, &type_string, true);
   summary.queue_type = sge_dstring_get_string(&type_string);
   summary.resv_slots = qinstance_slots_reserved_now(queue);
   summary.used_slots = qinstance_slots_used(queue);
   summary.total_slots = (int)queue.get<QU_job_slots>();

   /* arch */
   char arch_string[80];
   if (!sge_get_string_qattr(arch_string, sizeof(arch_string)-1, LOAD_ATTR_ARCH, queue, model.get_exechost_list(),
                             model.get_centry_list(), model.get_job_load_adjustments())) {
      summary.arch = arch_string;   // std::string copies; no longer aliases the stack array
   } else {
      summary.arch.clear();         // absent → empty (was nullptr)
   }

   DSTRING_STATIC(state_string, 32);
   qinstance_state_append_to_dstring(queue, &state_string);
   summary.state = sge_dstring_get_string(&state_string);

   // does the executing qstat user have access to this queue?
   const char *queue_name = queue.get<QU_full_name>();
   view.report_queue_summary(os, queue_name, &summary, parameter);

   if ((parameter.show_ & QSTAT_DISPLAY_ALARMREASON)) {
      if (*load_alarm_reason) {
         view.report_queue_load_alarm(os, queue_name, load_alarm_reason);
      }
      if (*suspend_alarm_reason) {
         view.report_queue_suspend_alarm(os,queue_name, suspend_alarm_reason);
      }
   }

   if ((parameter.explain_bits_ & QI_ALARM) > 0) {
      if (*load_alarm_reason) {
         view.report_queue_load_alarm(os,queue_name, load_alarm_reason);
      }
   }
   if ((parameter.explain_bits_ & QI_SUSPEND_ALARM) > 0) {
      if (*suspend_alarm_reason) {
         view.report_queue_suspend_alarm(os, queue_name, suspend_alarm_reason);
      }
   }
   if (parameter.explain_bits_ != QI_DEFAULT) {
      for (ocs::tom::ConstElement<ocs::tom::QIM> qim : queue.get<QU_message_list>()) {
         uint32_t type = qim.get<QIM_type>();

         if ((parameter.explain_bits_ & QI_AMBIGUOUS) == type || (parameter.explain_bits_ & QI_ERROR) == type) {
            const char *message = qim.get<QIM_message>();

            view.report_queue_message(os, queue_name, message);
         }
      }
   }

   /* view (selected) resources of queue in case of -F [attr,attr,..] */
   if (((parameter.show_ & QSTAT_DISPLAY_QRESOURCES))) {
      dstring resource_string = DSTRING_INIT;

      const auto rlp = queue_complexes2scheduler(queue, model.get_exechost_list(), model.get_centry_list(),
                                                 model.get_job_load_adjustments());

      bool in_resource_list = false;
      for (const auto rep : *rlp) {
         /* we had a -F request */
         if (const auto q_resource_list = parameter.get_q_resource_list(); q_resource_list) {
            auto qres = q_resource_list.find_first<CE_name>(rep.get<CE_name>());
            if (qres == nullptr) {
               qres = q_resource_list.find_first<CE_name>(rep.get<CE_shortcut>());
            }

            /* if this complex variable wasn't requested with -F, skip it */
            if (qres == nullptr) {
               continue ;
            }
         }

         sge_dstring_clear(&resource_string);
         char dom[5];
         uint32_t dominant = 0;
         const char *s = sge_get_dominant_stringval(rep, &dominant, &resource_string, nullptr, nullptr);
         monitor_dominance(dom, dominant);

         std::string details;
         if (strcmp(rep.get<CE_name>(), LOAD_ATTR_TOPOLOGY) == 0) {
            const char *hostname = queue.get<QU_qhostname>();
            const auto host = model.get_exechost_list().find_first<EH_name>(ocs::tom::Host(hostname));
            details = host_get_topology_in_use(host);
         }

         if (!in_resource_list) {
            view.report_queue_resource_started(os, "");
            in_resource_list = true;
         }
         view.report_queue_resource(os, rep, dom, rep.get<CE_name>(), s, details.c_str());
      }

      if (in_resource_list) {
         view.report_queue_resource_finished(os, "");
      }

      sge_dstring_free(&resource_string);

   }

   DRETURN_VOID;
}

/** @brief Run the request
 * @param parameter the parsed parameters
 * @param model the model, client or server side
 * @param view the view for the requested output format
 * @return none
 */
void ocs::QStatDefaultController::process_request(QStatParameter &parameter, QStatModelBase &model,
                                                  QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);

   model.calc_longest_queue_length(parameter);

   correct_capacities(model.get_exechost_list(), model.get_centry_list(), model.get_job_load_adjustments());

   view.report_started(out_, parameter);
   view.report_queue_section_started(out_, parameter);

   process_queues_with_its_jobs(out_, parameter, model, view);

   remove_tagged_jobs(model.get_job_list());

   // sort pending jobs
   if (!model.get_job_list().empty()) {
      Job::sgeee_sort_jobs(model.get_job_list());
   }

   view.report_queue_section_finished(out_, parameter);

   process_jobs_pending_state(out_, parameter, model, view);

   // @todo when will following two states be shown if all is printed as part of the pending section
   process_jobs_finished_state(out_, parameter, model, view);
   process_jobs_error_state(out_, parameter, model, view);

   view.report_finished(out_, parameter);

   DRETURN_VOID;
}
