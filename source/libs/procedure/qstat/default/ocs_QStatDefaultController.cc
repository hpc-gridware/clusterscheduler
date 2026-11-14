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
#include "sgeobj/tom/ocs_tom_BN.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_JRE.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_QIM.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_UA.h"

#include "sched/load_correction.h"
#include "sched/sge_complex_schedd.h"
#include "sched/sge_job_schedd.h"
#include "sched/sge_select_queue.h"
#include "sched/sge_urgency.h"

#include "qstat/default/ocs_QStatDefaultController.h"
#include "sched/sge_resource_utilization.h"

void ocs::QStatDefaultController::remove_tagged_jobs(lList *job_list) {

   lListElem *jep = lFirstRW(job_list);
   lListElem *tmp = nullptr;

   while (jep) {
      lListElem *jatep, *tmp_jatep;

      tmp = lNextRW(jep);
      ocs::tom::List<ocs::tom::JAT> task_list = ocs::tom::get<JB_ja_tasks>(jep);
      jatep = lFirstRW(task_list);
      while (jatep) {
         tmp_jatep = lNextRW(jatep);
         if ((ocs::tom::get<JAT_suitable>(jatep) & TAG_FOUND_IT)) {
            lRemoveElem(task_list, &jatep);
         }
         jatep = tmp_jatep;
      }
      jep = tmp;
   }

}

void
ocs::QStatDefaultController::process_queues_with_its_jobs(std::ostream &os, QStatParameter &parameter, QStatModelBase &model, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);

   /* no need to iterate through queues if queues are not printed */
   if (!parameter.need_queues_) {
      process_jobs_in_queue(os, nullptr, 1, parameter, model, view);
      DRETURN_VOID;
   }

   // handle running jobs of a queue
   for_each_rw_lv(qep, model.get_queue_list()) {

      const char* queue_name = ocs::tom::get<QU_full_name>(qep);

      // here we have the queue
      if (ocs::tom::get<QU_tag>(qep) & TAG_SHOW_IT) {
         if ((parameter.show_ & QSTAT_DISPLAY_NOEMPTYQ) && !qinstance_slots_used(qep)) {
            continue;
         }

         view.report_queue_started(os, queue_name, parameter);

         process_queue(os, qep, parameter, model, view);
         process_jobs_in_queue(os, qep, 1, parameter, model, view);

         view.report_queue_finished(os, queue_name, parameter);
      }
   }

   DRETURN_VOID;
}

void
ocs::QStatDefaultController::process_resources(std::ostream &os, const lList* cel, lList* centry_list, int slots, int scope,
                                                  bool is_hard_resource, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);

   if (cel == nullptr || lGetNumberOfElem(cel) == 0) {
       DRETURN_VOID;
   }

   if (is_hard_resource) {
      view.report_hard_resources_started(os, scope);
   } else {
      view.report_soft_resources_started(os, scope);
   }

   /* walk through complex entries */
   for_each_ep_lv(ce, cel) {
      double uc{0.0};
      const char *name = ocs::tom::get<CE_name>(ce);
      if (const lListElem *centry = centry_list_locate(centry_list, name); centry != nullptr) {
         uc = centry_urgency_contribution(slots, name, ocs::tom::get<CE_doubleval>(ce), centry);
      }

      const char *s = ocs::tom::get<CE_stringval>(ce);
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

void
ocs::QStatDefaultController::process_jobs_in_queue(std::ostream &os, lListElem *queue, bool print_jobs_of_queue, QStatParameter &parameter, QStatModelBase &model, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);
   uint32_t jid = 0, old_jid;
   uint32_t jataskid = 0, old_jataskid;
   const char *qnm = queue ? ocs::tom::get<QU_full_name>(queue) : nullptr;
   dstring dyn_task_str = DSTRING_INIT;
   bool started = false;

   for_each_rw_lv(jlep, model.get_job_list()) {
      int master, i;

      for (ocs::tom::Element<ocs::tom::JAT> jatep : ocs::tom::get<JB_ja_tasks>(jlep)) {
         uint32_t jstate = ocs::tom::get<JAT_state>(jatep);

         if (ISSET(jstate, JSUSPENDED_ON_SUBORDINATE) ||
             ISSET(jstate, JSUSPENDED_ON_SLOTWISE_SUBORDINATE)) {
            ocs::tom::set<JAT_state>(jatep, jstate & ~JRUNNING);
         }

         lListElem *old_gdilep = nullptr;
         for (ocs::tom::Element<ocs::tom::JG> gdilep : ocs::tom::get<JAT_granted_destin_identifier_list>(jatep)) {

            if (!queue || !strcmp(ocs::tom::get<JG_qname>(gdilep), qnm)) {
               int slot_adjust = 0;
               int lines_to_print;
               int slots_per_line = 0;
               const u_long raw_slots = ocs::tom::get<JG_slots>(gdilep);
               int slots_in_queue = raw_slots > static_cast<u_long>(std::numeric_limits<int>::max())
                                    ? std::numeric_limits<int>::max()
                                    : static_cast<int>(raw_slots);

               if (!queue)
                  qnm = ocs::tom::get<JG_qname>(gdilep);

               uint32_t job_tag = ocs::tom::get<JAT_suitable>(jatep);
               job_tag |= TAG_FOUND_IT;
               ocs::tom::set<JAT_suitable>(jatep, job_tag);

               master = !strcmp(qnm,
                     ocs::tom::get<JG_qname>(lFirst(static_cast<const lList *>(ocs::tom::get<JAT_granted_destin_identifier_list>(jatep)))));

               if (master) {
                  const char *pe_name;
                  lListElem *pe;
                  if (((pe_name=ocs::tom::get<JAT_granted_pe>(jatep))) &&
                      ((pe=pe_list_locate(model.get_pe_list(), pe_name))) &&
                      !ocs::tom::get<PE_job_is_first_task>(pe))

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
                     slots_per_line = sge_granted_slots(ocs::tom::get<JAT_granted_destin_identifier_list>(jatep));
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
                  jid = ocs::tom::get<JB_job_number>(jlep);
                  old_jataskid = jataskid;
                  jataskid = ocs::tom::get<JAT_task_number>(jatep);

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

                  if (!lGetNumberOfElem(parameter.get_user_list()) ||
                     (lGetNumberOfElem(parameter.get_user_list()) && (ocs::tom::get<JAT_suitable>(jatep)&TAG_SELECT_IT))) {
                     if (print_jobs_of_queue && (job_tag & TAG_SHOW_IT)) {
                        if ((parameter.show_ & QSTAT_DISPLAY_RUNNING) && (ocs::tom::get<JAT_state>(jatep) & JRUNNING) ) {
                           print_it = true;
                        } else if ((parameter.show_ & QSTAT_DISPLAY_SUSPENDED) &&
                           ((ocs::tom::get<JAT_state>(jatep)&JSUSPENDED) || (ocs::tom::get<JAT_state>(jatep)&JSUSPENDED_ON_THRESHOLD) ||
                           (ocs::tom::get<JAT_state>(jatep)&JSUSPENDED_ON_SUBORDINATE) || (ocs::tom::get<JAT_state>(jatep)&JSUSPENDED_ON_SLOTWISE_SUBORDINATE))) {
                           print_it = true;
                        } else if ((parameter.show_ & QSTAT_DISPLAY_USERHOLD) && (ocs::tom::get<JAT_hold>(jatep)&MINUS_H_TGT_USER)) {
                           print_it = true;
                        } else if ((parameter.show_ & QSTAT_DISPLAY_OPERATORHOLD) && (ocs::tom::get<JAT_hold>(jatep)&MINUS_H_TGT_OPERATOR))  {
                           print_it = true;
                        } else if ((parameter.show_ & QSTAT_DISPLAY_SYSTEMHOLD) && (ocs::tom::get<JAT_hold>(jatep)&MINUS_H_TGT_SYSTEM)) {
                           print_it = true;
                        } else if ((parameter.show_ & QSTAT_DISPLAY_JOBARRAYHOLD) && (ocs::tom::get<JAT_hold>(jatep)&MINUS_H_TGT_JA_AD)) {
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
                           process_job(os, jlep, jatep, queue, gdilep, print_jobid, (master && different && (i==0))?"MASTER":"SLAVE",
                                          &dyn_task_str, slots_in_queue+slot_adjust, i, slots_per_line, parameter, model, view);
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

void ocs::QStatDefaultController::process_job(std::ostream &os, lListElem *job, lListElem *jatep, lListElem *qep, lListElem *gdil_ep, bool print_jobid,
                                              const char *master, dstring *dyn_task_str, int slots, int slot, int slots_per_line,
                                              QStatParameter &parameter, QStatModelBase &model, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);
   uint32_t jstate;
   const lList *ql = nullptr;
   QStatDefaultViewBase::job_summary_t summary{};

   summary.print_jobid = print_jobid;

   if (gdil_ep) {
      summary.queue = ocs::tom::get<JG_qname>(gdil_ep);
   }

   const bool sge_ext = (parameter.show_ & QSTAT_DISPLAY_EXTENDED) ? true : false;
   const bool tsk_ext = (parameter.show_ & QSTAT_DISPLAY_TASKS) ? true : false;
   const bool sge_urg = (parameter.show_ & QSTAT_DISPLAY_URGENCY) ? true : false;
   const bool sge_pri = (parameter.show_ & QSTAT_DISPLAY_PRIORITY) ? true : false;
   const bool sge_time = !sge_ext | tsk_ext | sge_urg | sge_pri;

   summary.nprior = ocs::tom::get<JAT_prio>(jatep);
   if (sge_pri || sge_urg) {
      summary.nurg = ocs::tom::get<JB_nurg>(job);
   }
   if (sge_pri) {
      summary.nppri = ocs::tom::get<JB_nppri>(job);
   }
   if (sge_pri || sge_ext) {
      summary.ntckts =  ocs::tom::get<JAT_ntix>(jatep);
   }
   if (sge_urg) {
      summary.urg = ocs::tom::get<JB_urg>(job);
      summary.rrcontr = ocs::tom::get<JB_rrcontr>(job);
      summary.wtcontr = ocs::tom::get<JB_wtcontr>(job);
      summary.dlcontr = ocs::tom::get<JB_dlcontr>(job);
   }
   if (sge_pri) {
      summary.priority = static_cast<int>(ocs::tom::get<JB_priority>(job)) - BASE_PRIORITY;
   }
   summary.name = ocs::tom::get<JB_job_name>(job);
   summary.user = ocs::tom::get<JB_owner>(job);
   if (sge_ext) {
      summary.project = ocs::tom::get<JB_project>(job);
      summary.department = ocs::tom::get<JB_department>(job);
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
      summary.submit_time = ocs::tom::get<JB_submission_time>(job);
      summary.start_time = ocs::tom::get<JAT_start_time>(jatep);
   }

   if (ocs::tom::get<JAT_status>(jatep)==JRUNNING || ocs::tom::get<JAT_status>(jatep)==JTRANSFERING) {
      summary.is_running = true;
   } else {
      summary.is_running = false;
   }

   if (sge_urg) {
      summary.deadline = ocs::tom::get<JB_deadline>(job);
   }

   if (sge_ext) {
      const lListElem *up, *pe;
      lList *job_usage_list;
      const char *pe_name;
      bool sum_pe_tasks = false;

      if (master == nullptr || strcmp(master, "MASTER") == 0) {
         if (!(parameter.group_opt_ & GROUP_NO_PETASK_GROUPS)) {
            sum_pe_tasks = true;
         }
         job_usage_list = lCopyList(nullptr, ocs::tom::get<JAT_scaled_usage_list>(jatep));
      } else {
         job_usage_list = lCreateList("", UA_Type);
      }

      /* sum pe-task usage based on queue slots */
      if (job_usage_list) {
         int subtask_ndx=1;
         for (ocs::tom::ConstElement<ocs::tom::PET> task : ocs::tom::get<JAT_task_list>(jatep)) {
            ocs::tom::Element<ocs::tom::UA> dst;
            ocs::tom::ConstElement<ocs::tom::JG> ep;
            const char *qname;

            if (sum_pe_tasks ||
                (summary.queue &&
                 ((ep=lFirst(ocs::tom::get<PET_granted_destin_identifier_list>(task)))) &&
                 ((qname=ocs::tom::get<JG_qname>(ep))) &&
                 !strcmp(qname, summary.queue) && ((subtask_ndx++%slots)==slot))) {
               for (ocs::tom::ConstElement<ocs::tom::UA> src : ocs::tom::get<PET_scaled_usage>(task)) {
                  if ((dst=ocs::tom::find_first_rw<UA_name>(job_usage_list, ocs::tom::get<UA_name>(src))))
                     ocs::tom::set<UA_value>(dst, ocs::tom::get<UA_value>(dst) + ocs::tom::get<UA_value>(src));
                  else
                     lAppendElem(job_usage_list, lCopyElem(src));
               }
            }
         }
      }


      /* scaled cpu usage */
      if (!(up = ocs::tom::find_first<UA_name>(job_usage_list, USAGE_ATTR_CPU))) {
         summary.has_cpu_usage = false;
      } else {
         summary.has_cpu_usage = true;
         summary.cpu_usage = ocs::tom::get<UA_value>(up);
      }
      /* scaled mem usage */
      if (!(up = ocs::tom::find_first<UA_name>(job_usage_list, USAGE_ATTR_MEM))) {
         summary.has_mem_usage = false;
      } else {
         summary.has_mem_usage = true;
         summary.mem_usage = ocs::tom::get<UA_value>(up);
      }
      /* scaled io usage */
      if (!(up = ocs::tom::find_first<UA_name>(job_usage_list, USAGE_ATTR_IO))) {
         summary.has_io_usage = false;
      } else {
         summary.has_io_usage = true;
         summary.io_usage = ocs::tom::get<UA_value>(up);
      }
      lFreeList(&job_usage_list);

      /* get tickets for job/slot */

      summary.override_tickets = ocs::tom::get<JB_override_tickets>(job);
      summary.share = ocs::tom::get<JAT_share>(jatep);
      if (sge_ext) {
         summary.is_queue_assigned = false;
      } else {
         if (static_cast<const lList *>(ocs::tom::get<JAT_granted_destin_identifier_list>(jatep)) != nullptr) {
            summary.is_queue_assigned = true;
         } else {
            summary.is_queue_assigned = false;
         }
      }

      /* braces needed to suppress compiler warnings */
      if ((pe_name=ocs::tom::get<JAT_granted_pe>(jatep)) &&
           (pe=pe_list_locate(model.get_pe_list(), pe_name)) &&
           ocs::tom::get<PE_control_slaves>(pe) && slots) {
         if (slot == 0) {
            summary.tickets = (u_long)ocs::tom::get<JG_ticket>(gdil_ep);
            summary.otickets = (u_long)ocs::tom::get<JG_oticket>(gdil_ep);
            summary.ftickets = (u_long)ocs::tom::get<JG_fticket>(gdil_ep);
            summary.stickets = (u_long)ocs::tom::get<JG_sticket>(gdil_ep);
         }
         else {
            if (slots) {
               summary.tickets = (u_long)(ocs::tom::get<JG_ticket>(gdil_ep) / slots);
               summary.otickets = (u_long)(ocs::tom::get<JG_oticket>(gdil_ep) / slots);
               summary.ftickets = (u_long)(ocs::tom::get<JG_fticket>(gdil_ep) / slots);
               summary.stickets = (u_long)(ocs::tom::get<JG_sticket>(gdil_ep) / slots);
            }
            else {
               summary.tickets = summary.otickets = summary.ftickets = summary.stickets = 0;
            }
         }
      }
      else {
         summary.tickets = (u_long)ocs::tom::get<JAT_tix>(jatep);
         summary.otickets = (u_long)ocs::tom::get<JAT_oticket>(jatep);
         summary.ftickets = (u_long)ocs::tom::get<JAT_fticket>(jatep);
         summary.stickets = (u_long)ocs::tom::get<JAT_sticket>(jatep);
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

   view.report_job(os, ocs::tom::get<JB_job_number>(job), &summary, parameter, model);

   if (tsk_ext) {
      ocs::tom::ConstList<ocs::tom::PET> task_list = ocs::tom::get<JAT_task_list>(jatep);
      ocs::tom::ConstElement<ocs::tom::JG> ep;
      const char *qname;
      int subtask_ndx=1;
      bool close_tasks{false};

      /* print master sub-task belonging to this queue */
      if (!slot && task_list && summary.queue &&
          ((ep=lFirst(ocs::tom::get<JAT_granted_destin_identifier_list>(jatep)))) &&
          ((qname=ocs::tom::get<JG_qname>(ep))) &&
          !strcmp(qname, summary.queue)) {

         if (!close_tasks) {
            view.report_sub_tasks_started(os);
            close_tasks = true;
         }

         process_subtask(os, job, jatep, nullptr, view);
      }

      /* print sub-tasks belonging to this queue */
      for_each_rw_lv(task, task_list) {
         if (!slots || (summary.queue &&
              ((ep=lFirst(ocs::tom::get<PET_granted_destin_identifier_list>(task)))) &&
              ((qname=ocs::tom::get<JG_qname>(ep))) &&
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

      view.report_additional_info(os, QStatDefaultViewBase::FULL_JOB_NAME, ocs::tom::get<JB_job_name>(job));
      if (summary.queue) {
         view.report_additional_info(os, QStatDefaultViewBase::MASTER_QUEUE, summary.queue);
      }

      if (ocs::tom::get<JB_pe>(job)) {
         dstring range_string = DSTRING_INIT;

         range_list_print_to_string(ocs::tom::get<JB_pe_range>(job),
                                    &range_string, true, false, false);

         view.report_requested_pe(os, ocs::tom::get<JB_pe>(job), sge_dstring_get_string(&range_string));

         sge_dstring_free(&range_string);
      }

      if (ocs::tom::get<JAT_granted_pe>(jatep)) {
         uint32_t pe_slots = 0;
         for (ocs::tom::ConstElement<ocs::tom::JG> l_gdil_ep : ocs::tom::get<JAT_granted_destin_identifier_list>(jatep)) {
            pe_slots += ocs::tom::get<JG_slots>(l_gdil_ep);
         }

         view.report_granted_pe(os, ocs::tom::get<JAT_granted_pe>(jatep), pe_slots);
      }
      if (ocs::tom::get<JB_checkpoint_name>(job)) {
         view.report_additional_info(os, QStatDefaultViewBase::CHECKPOINT_ENV, ocs::tom::get<JB_checkpoint_name>(job));
      }

      /* Handle the Hard Resources (global, master, slave) */
      process_resources(os, job_get_hard_resource_list(job, JRS_SCOPE_GLOBAL), model.get_centry_list(),
                           sge_job_slot_request(job, model.get_pe_list()), JRS_SCOPE_GLOBAL, true, view);

      process_resources(os, job_get_hard_resource_list(job, JRS_SCOPE_MASTER), model.get_centry_list(),
                           sge_job_slot_request(job, model.get_pe_list()), JRS_SCOPE_MASTER, true, view);

      process_resources(os, job_get_hard_resource_list(job, JRS_SCOPE_SLAVE), model.get_centry_list(),
                           sge_job_slot_request(job, model.get_pe_list()),
                           JRS_SCOPE_SLAVE, true, view);

      /* display default requests if necessary */
      {
         lList *attributes = nullptr;
         const char *name;
         lListElem *hep;
         bool first_default_request{true};

         queue_complexes2scheduler(&attributes, qep, model.get_exechost_list(), model.get_centry_list(),
                                   model.get_job_load_adjustments());
         for_each_ep_lv(ce, attributes) {
            double dval;

            name = ocs::tom::get<CE_name>(ce);
            if (!ocs::tom::get<CE_consumable>(ce) || !strcmp(name, "slots") ||
                job_get_request(job, name)) {
               continue;
            }

            parse_ulong_val(&dval, nullptr, static_cast<CEntry::Type>(ocs::tom::get<CE_valtype>(ce)), ocs::tom::get<CE_defaultval>(ce), nullptr, 0);
            if (dval == 0.0) {
               continue;
            }

            /* For pending jobs (no queue/no exec host) we may print default request only
               if the consumable is specified in the global host. For running we print it
               if the resource is managed at this node/queue */
            if ((qep && ocs::tom::find_first_in<QU_consumable_config_list, CE_name>(qep, name)) ||
                (qep && (hep=host_list_locate(model.get_exechost_list(), ocs::tom::get<QU_qhostname>(qep))) &&
                 ocs::tom::find_first_in<EH_consumable_config_list, CE_name>(hep, name)) ||
                  ((hep=host_list_locate(model.get_exechost_list(), SGE_GLOBAL_NAME)) &&
                  ocs::tom::find_first_in<EH_consumable_config_list, CE_name>(hep, name))) {
               if (first_default_request) {
                  view.report_default_request_started(os);
                  first_default_request = false;
               }
               view.report_default_request(os, name, ocs::tom::get<CE_defaultval>(ce));
            }
         }
         if (!first_default_request) {
            view.report_default_request_finished(os);
         }
         lFreeList(&attributes);
      }

      /* Handle the Soft Resources */
      process_resources(os, job_get_soft_resource_list(job), model.get_centry_list(),
                           sge_job_slot_request(job, model.get_pe_list()), JRS_SCOPE_GLOBAL, false, view);

      {
         ql = job_get_hard_queue_list(job, JRS_SCOPE_GLOBAL);
         if (ql != nullptr && lGetNumberOfElem(ql) != 0) {
            view.report_hard_requested_queues_started(os, JRS_SCOPE_GLOBAL);
            for_each_ep_lv(qrep, ql) {
               view.report_hard_requested_queue(os, JRS_SCOPE_GLOBAL, ocs::tom::get<QR_name>(qrep));
            }
            view.report_hard_requested_queues_finished(os);
         }
         ql = job_get_hard_queue_list(job, JRS_SCOPE_MASTER);
         if (ql != nullptr && lGetNumberOfElem(ql) != 0) {
            view.report_hard_requested_queues_started(os, JRS_SCOPE_MASTER);
            for_each_ep_lv(qrep, ql) {
               view.report_hard_requested_queue(os, JRS_SCOPE_MASTER, ocs::tom::get<QR_name>(qrep));
            }
            view.report_hard_requested_queues_finished(os);
         }
         ql = job_get_hard_queue_list(job, JRS_SCOPE_SLAVE);
         if (ql != nullptr && lGetNumberOfElem(ql) != 0) {
            view.report_hard_requested_queues_started(os, JRS_SCOPE_SLAVE);
            for_each_ep_lv(qrep, ql) {
               view.report_hard_requested_queue(os, JRS_SCOPE_SLAVE, ocs::tom::get<QR_name>(qrep));
            }
            view.report_hard_requested_queues_finished(os);
         }
      }

      {
         ql = job_get_soft_queue_list(job, JRS_SCOPE_GLOBAL);
         if (ql != nullptr && lGetNumberOfElem(ql) != 0) {
            view.report_soft_requested_queues_started(os,JRS_SCOPE_GLOBAL);
            for_each_ep_lv(qrep, ql) {
               view.report_soft_requested_queue(os, JRS_SCOPE_GLOBAL, ocs::tom::get<QR_name>(qrep));
            }
            view.report_soft_requested_queues_finished(os);
         }
         ql = job_get_soft_queue_list(job, JRS_SCOPE_MASTER);
         if (ql != nullptr && lGetNumberOfElem(ql) != 0) {
            view.report_soft_requested_queues_started(os, JRS_SCOPE_MASTER);
            for_each_ep_lv(qrep, ql) {
               view.report_soft_requested_queue(os, JRS_SCOPE_MASTER, ocs::tom::get<QR_name>(qrep));
            }
            view.report_soft_requested_queues_finished(os);
         }
         ql = job_get_soft_queue_list(job, JRS_SCOPE_SLAVE);
         if (ql != nullptr && lGetNumberOfElem(ql) != 0) {
            view.report_soft_requested_queues_started(os, JRS_SCOPE_SLAVE);
            for_each_ep_lv(qrep, ql) {
               view.report_soft_requested_queue(os, JRS_SCOPE_SLAVE, ocs::tom::get<QR_name>(qrep));
            }
            view.report_soft_requested_queues_finished(os);
         }
      }

      {
         ocs::tom::ConstList<ocs::tom::JRE> ql = ocs::tom::get<JB_jid_request_list>(job );
         if (ql) {
            view.report_predecessors_requested_started(os);
            for (ocs::tom::ConstElement<ocs::tom::JRE> qrep : ql) {
               view.report_predecessor_requested(os, ocs::tom::get<JRE_job_name>(qrep));
            }
            view.report_predecessors_requested_finished(os);
         }
      }
      {
         ocs::tom::ConstList<ocs::tom::JRE> ql = ocs::tom::get<JB_jid_predecessor_list>(job);
         if (ql) {
            view.report_predecessors_started(os);
            for (ocs::tom::ConstElement<ocs::tom::JRE> qrep : ql) {
               view.report_predecessor(os, ocs::tom::get<JRE_job_number>(qrep));
            }
            view.report_predecessors_finished(os);
         }
      }

      {
         ocs::tom::ConstList<ocs::tom::JRE> ql = ocs::tom::get<JB_ja_ad_request_list>(job );
         if (ql) {
            view.report_ad_predecessors_requested_started(os);
            for (ocs::tom::ConstElement<ocs::tom::JRE> qrep : ql) {
               view.report_ad_predecessor_requested(os, ocs::tom::get<JRE_job_name>(qrep));
            }
            view.report_ad_predecessors_requested_finished(os);
         }
      }
      {
         ocs::tom::ConstList<ocs::tom::JRE> ql = ocs::tom::get<JB_ja_ad_predecessor_list>(job);
         if (ql) {
            view.report_ad_predecessors_started(os);
            for (ocs::tom::ConstElement<ocs::tom::JRE> qrep : ql) {
               view.report_ad_predecessor(os, ocs::tom::get<JRE_job_number>(qrep));
            }
            view.report_ad_predecessors_finished(os);
         }
      }

      if (ocs::tom::ConstElement<ocs::tom::BN> binding_elem = ocs::tom::get<JB_binding>(job); binding_elem != nullptr) {
         dstring binding_param = DSTRING_INIT;

         // all binding specific parameter as one string
         std::string binding_str;
         BindingIo::binding_print_to_string(binding_elem, binding_str);
         sge_dstring_sprintf(&binding_param, "%s", binding_str.c_str());

         view.report_binding_started(os);
         view.report_binding(os, sge_dstring_get_string(&binding_param));
         view.report_binding_attribute(os, "bamount", ocs::tom::get<BN_amount>(binding_elem));
         view.report_binding_attribute(os, "binstance", BindingInstance::to_string(static_cast<BindingInstance::Instance>(ocs::tom::get<BN_instance>(binding_elem))).c_str());
         view.report_binding_attribute(os, "bstrategy", BindingStrategy::to_string(static_cast<BindingStrategy::Strategy>(ocs::tom::get<BN_strategy>(binding_elem))).c_str());
         view.report_binding_attribute(os, "btype", BindingType::to_string(static_cast<BindingType::Type>(ocs::tom::get<BN_new_type>(binding_elem))).c_str());
         view.report_binding_attribute(os, "bunit", BindingUnit::to_string(static_cast<BindingUnit::Unit>(ocs::tom::get<BN_unit>(binding_elem))).c_str());
         view.report_binding_attribute(os, "bfilter", ocs::tom::get<BN_filter>(binding_elem));
         view.report_binding_attribute(os, "bsort", ocs::tom::get<BN_sort>(binding_elem));
         view.report_binding_attribute(os, "bstart", BindingStart::to_string(static_cast<BindingStart::Start>(ocs::tom::get<BN_start>(binding_elem))).c_str());
         view.report_binding_attribute(os, "bstop", BindingStop::to_string(static_cast<BindingStop::Stop>(ocs::tom::get<BN_stop>(binding_elem))).c_str());
         view.report_binding_finished(os);
         sge_dstring_free(&binding_param);
      }
   }

   view.report_job_finished(os, ocs::tom::get<JB_job_number>(job));

   DRETURN_VOID;
}

void
ocs::QStatDefaultController::process_subtask(std::ostream &os, lListElem *job, lListElem *ja_task, lListElem *pe_task, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);
   char task_state_string[8];
   uint32_t tstate, tstatus;
   ocs::tom::ConstList<ocs::tom::UA> usage_list;
   ocs::tom::ConstList<ocs::tom::UA> scaled_usage_list;

   QStatDefaultViewBase::task_summary_t summary{};

   /* is sub-task logically running */
   if (pe_task == nullptr) {
      tstatus = ocs::tom::get<JAT_status>(ja_task);
      usage_list = ocs::tom::get<JAT_usage_list>(ja_task);
      scaled_usage_list = ocs::tom::get<JAT_scaled_usage_list>(ja_task);
   } else {
      tstatus = ocs::tom::get<PET_status>(pe_task);
      usage_list = ocs::tom::get<PET_usage>(pe_task);
      scaled_usage_list = ocs::tom::get<PET_scaled_usage>(pe_task);
   }

   if (pe_task == nullptr) {
      summary.task_id = "";
   } else {
      summary.task_id = ocs::tom::get<PET_id>(pe_task);
   }

   /* move status info into state info */
   tstate = ocs::tom::get<JAT_state>(ja_task);
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

   if (ocs::tom::get<JB_jid_predecessor_list>(job) || ocs::tom::get<JAT_hold>(ja_task)) {
      tstate |= JHELD;
   }

   if (ocs::tom::get<JAT_job_restarted>(ja_task)) {
      tstate &= ~JWAITING;
      tstate |= JMIGRATING;
   }

   /* write states into string */
   job_get_state_string(task_state_string, tstate);
   summary.state = task_state_string;

   {
      ocs::tom::ConstElement<ocs::tom::UA> up;

      /* scaled cpu usage */
      if (!(up = ocs::tom::find_first<UA_name>(scaled_usage_list, USAGE_ATTR_CPU))) {
         summary.has_cpu_usage = false;
      } else {
         summary.has_cpu_usage = true;
         summary.cpu_usage = ocs::tom::get<UA_value>(up);
      }

      /* scaled mem usage */
      if (!(up = ocs::tom::find_first<UA_name>(scaled_usage_list, USAGE_ATTR_MEM))) {
         summary.has_mem_usage = false;
      } else {
         summary.has_mem_usage = true;
         summary.mem_usage = ocs::tom::get<UA_value>(up);
      }

      /* scaled io usage */
      if (!(up = ocs::tom::find_first<UA_name>(scaled_usage_list, USAGE_ATTR_IO))) {
         summary.has_io_usage = false;
      } else {
         summary.has_io_usage = true;
         summary.io_usage = ocs::tom::get<UA_value>(up);
      }
   }

   if (tstatus==JFINISHED) {
      ocs::tom::ConstElement<ocs::tom::UA> ep=ocs::tom::find_first<UA_name>(usage_list, "exit_status");
      if (ep) {
         summary.has_exit_status = true;
         summary.exit_status = ocs::tom::get<UA_value>(ep);
      } else {
         summary.has_exit_status = false;
      }
   } else {
      summary.has_exit_status = false;
   }

   view.report_sub_task(os, &summary);

   DRETURN_VOID;
}

void ocs::QStatDefaultController::process_jobs_pending_state(std::ostream &os, QStatParameter &parameter, QStatModelBase &model, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);

   lListElem *nxt, *jep, *jatep, *nxt_jatep;
   lList* ja_task_list = nullptr;
   int FoundTasks;
   int count = 0;
   dstring dyn_task_str = DSTRING_INIT;

   nxt = lFirstRW(model.get_job_list());
   while ((jep=nxt)) {
      nxt = lNextRW(jep);
      nxt_jatep = lFirstRW(static_cast<const lList *>(ocs::tom::get<JB_ja_tasks>(jep)));
      FoundTasks = 0;

      bool hide_data = !job_is_visible(parameter.get_packet(), ocs::tom::get<JB_owner>(jep), model.is_manager());
      if (hide_data) {
         continue;
      }

      while ((jatep = nxt_jatep)) {
         nxt_jatep = lNextRW(jatep);

         if (!(((parameter.show_ & QSTAT_DISPLAY_OPERATORHOLD) && (ocs::tom::get<JAT_hold>(jatep)&MINUS_H_TGT_OPERATOR))
               ||
             ((parameter.show_ & QSTAT_DISPLAY_USERHOLD) && (ocs::tom::get<JAT_hold>(jatep)&MINUS_H_TGT_USER))
               ||
             ((parameter.show_ & QSTAT_DISPLAY_SYSTEMHOLD) && (ocs::tom::get<JAT_hold>(jatep)&MINUS_H_TGT_SYSTEM))
               ||
             ((parameter.show_ & QSTAT_DISPLAY_JOBARRAYHOLD) && (ocs::tom::get<JAT_hold>(jatep)&MINUS_H_TGT_JA_AD))
               ||
             ((parameter.show_ & QSTAT_DISPLAY_JOBHOLD) && ocs::tom::get<JB_jid_predecessor_list>(jep))
               ||
             ((parameter.show_ & QSTAT_DISPLAY_STARTTIMEHOLD) && ocs::tom::get<JB_execution_time>(jep))
               ||
             !(parameter.show_ & QSTAT_DISPLAY_HOLD))
            ) {
            break;
         }

         if (!(ocs::tom::get<JAT_suitable>(jatep) & TAG_FOUND_IT) &&
            VALID(JQUEUED, ocs::tom::get<JAT_state>(jatep)) &&
            !VALID(JFINISHED, ocs::tom::get<JAT_status>(jatep))) {
            ocs::tom::set<JAT_suitable>(jatep,
            ocs::tom::get<JAT_suitable>(jatep)|TAG_FOUND_IT);

            if ((!lGetNumberOfElem(parameter.get_user_list()) || (lGetNumberOfElem(parameter.get_user_list()) && (ocs::tom::get<JAT_suitable>(jatep)&TAG_SELECT_IT))) &&
                (ocs::tom::get<JAT_suitable>(jatep)&TAG_SHOW_IT)) {

               if ((parameter.show_ & QSTAT_DISPLAY_PENDING) &&
                   (parameter.group_opt_ & GROUP_NO_TASK_GROUPS) > 0) {

                  sge_dstring_sprintf(&dyn_task_str, sge_u32, ocs::tom::get<JAT_task_number>(jatep));

                  if (count == 0) {
                     view.report_pending_jobs_started(os, parameter);
                  }
                  process_job(os, jep, jatep, nullptr, nullptr, true, nullptr, &dyn_task_str, 0, 0, 0, parameter, model, view);

                  count++;
               } else {
                  if (!ja_task_list) {
                     ja_task_list = lCreateList("", lGetElemDescr(jatep));
                  }
                  lAppendElem(ja_task_list, lCopyElem(jatep));
                  FoundTasks = 1;
               }
            }
         }
      }
      if ((parameter.show_ & QSTAT_DISPLAY_PENDING)  &&
          (parameter.group_opt_ & GROUP_NO_TASK_GROUPS) == 0 &&
          FoundTasks &&
          ja_task_list) {
         lList *task_group = nullptr;

         while ((task_group = ja_task_list_split_group(&ja_task_list))) {
            sge_dstring_clear(&dyn_task_str);
            ja_task_list_print_to_string(task_group, &dyn_task_str);

            if (count == 0) {
               view.report_pending_jobs_started(os, parameter);
            }
            process_job(os, jep, lFirstRW(task_group), nullptr, nullptr, true, nullptr, &dyn_task_str, 0, 0, 0, parameter, model, view);

            lFreeList(&task_group);

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
   lFreeList(&ja_task_list);
   DRETURN_VOID;
}


void ocs::QStatDefaultController::process_jobs_finished_state(std::ostream &os, QStatParameter &parameter, QStatModelBase &model, QStatDefaultViewBase &view) {
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

   for_each_rw_lv(jep, model.get_job_list()) {
      lList *ja_task_list = nullptr;
      bool FoundTasks = false;

      for (ocs::tom::Element<ocs::tom::JAT> jatep : ocs::tom::get<JB_ja_tasks>(jep)) {
         if (ocs::tom::get<JAT_status>(jatep) != JFINISHED) {
            continue;
         }
         ocs::tom::set<JAT_suitable>(jatep, ocs::tom::get<JAT_suitable>(jatep) | TAG_FOUND_IT);

         if (lGetNumberOfElem(parameter.get_user_list()) &&
             !(ocs::tom::get<JAT_suitable>(jatep) & TAG_SELECT_IT)) {
            continue;
         }

         /* CS-1908 U7: -g d drills down to per-JAT rows; otherwise range-group
          * the finished JATs of this JB into one summary row (state character
          * `f`, JAT-id-range in the ja-task-ID column). Mirrors the pending
          * path's summary-vs-detail fork. */
         if ((parameter.group_opt_ & GROUP_NO_TASK_GROUPS) > 0) {
            sge_dstring_sprintf(&dyn_task_str, sge_u32, ocs::tom::get<JAT_task_number>(jatep));

            if (count == 0) {
               view.report_finished_jobs_started(os, parameter);
            }
            process_job(os, jep, jatep, nullptr, nullptr, true, nullptr, &dyn_task_str, 0, 0, 0, parameter, model, view);

            count++;
         } else {
            if (ja_task_list == nullptr) {
               ja_task_list = lCreateList("", lGetElemDescr(jatep));
            }
            lAppendElem(ja_task_list, lCopyElem(jatep));
            FoundTasks = true;
         }
      }

      if ((parameter.group_opt_ & GROUP_NO_TASK_GROUPS) == 0 && FoundTasks && ja_task_list != nullptr) {
         lList *task_group = nullptr;

         while ((task_group = ja_task_list_split_group(&ja_task_list))) {
            sge_dstring_clear(&dyn_task_str);
            ja_task_list_print_to_string(task_group, &dyn_task_str);

            if (count == 0) {
               view.report_finished_jobs_started(os, parameter);
            }
            process_job(os, jep, lFirstRW(task_group), nullptr, nullptr, true, nullptr, &dyn_task_str, 0, 0, 0, parameter, model, view);

            lFreeList(&task_group);

            count++;
         }
      }
      lFreeList(&ja_task_list);
   }

   if (count > 0) {
      view.report_finished_jobs_finished(os);
   }

   sge_dstring_free(&dyn_task_str);
   DRETURN_VOID;
}


void ocs::QStatDefaultController::process_jobs_error_state(std::ostream &os, QStatParameter &parameter, QStatModelBase &model, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);
   int count = 0;
   dstring dyn_task_str = DSTRING_INIT;

   for_each_rw_lv(jep, model.get_job_list()) {
      for (ocs::tom::Element<ocs::tom::JAT> jatep : ocs::tom::get<JB_ja_tasks>(jep)) {
         if (!(ocs::tom::get<JAT_suitable>(jatep) & TAG_FOUND_IT) && ocs::tom::get<JAT_status>(jatep) == JERROR) {
            ocs::tom::set<JAT_suitable>(jatep, ocs::tom::get<JAT_suitable>(jatep)|TAG_FOUND_IT);

            if (!lGetNumberOfElem(parameter.get_user_list()) || (lGetNumberOfElem(parameter.get_user_list()) && (ocs::tom::get<JAT_suitable>(jatep)&TAG_SELECT_IT))) {
               sge_dstring_sprintf(&dyn_task_str, sge_u32, ocs::tom::get<JAT_task_number>(jatep));

               if (count == 0) {
                   view.report_error_jobs_started(os, parameter);
               }
               process_job(os, jep, jatep, nullptr, nullptr, true, nullptr, &dyn_task_str, 0, 0, 0, parameter, model, view);

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

void ocs::QStatDefaultController::process_jobs_not_enrolled(std::ostream &os, lListElem *job, bool print_jobid, char *master,
                                                            int slots, int slot, int *count,
                                                            QStatParameter &parameter, QStatModelBase &model, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);
   lList *range_list[16];         /* RN_Type */
   uint32_t hold_state[16];
   int i;
   dstring ja_task_id_string = DSTRING_INIT;


   job_create_hold_id_lists(job, range_list, hold_state);
   for (i = 0; i <= 15; i++) {
      lList *answer_list = nullptr;
      uint32_t first_id;
      int show = 0;

      if (((parameter.show_ & QSTAT_DISPLAY_USERHOLD) && (hold_state[i] & MINUS_H_TGT_USER)) ||
          ((parameter.show_ & QSTAT_DISPLAY_OPERATORHOLD) && (hold_state[i] & MINUS_H_TGT_OPERATOR)) ||
          ((parameter.show_ & QSTAT_DISPLAY_SYSTEMHOLD) && (hold_state[i] & MINUS_H_TGT_SYSTEM)) ||
          ((parameter.show_ & QSTAT_DISPLAY_JOBARRAYHOLD) && (hold_state[i] & MINUS_H_TGT_JA_AD)) ||
          ((parameter.show_ & QSTAT_DISPLAY_STARTTIMEHOLD) && (ocs::tom::get<JB_execution_time>(job) > 0)) ||
          ((parameter.show_ & QSTAT_DISPLAY_JOBHOLD) && (static_cast<const lList *>(ocs::tom::get<JB_jid_predecessor_list>(job)) != 0)) ||
          (!(parameter.show_ & QSTAT_DISPLAY_HOLD))
         ) {
         show = 1;
      }
      if (range_list[i] != nullptr && show) {
         if ((parameter.group_opt_ & GROUP_NO_TASK_GROUPS) == 0) {
            sge_dstring_clear(&ja_task_id_string);
            range_list_print_to_string(range_list[i], &ja_task_id_string, false, false, false);
            first_id = range_list_get_first_id(range_list[i], &answer_list);
            if (answer_list_has_error(&answer_list) != 1) {
               lListElem *ja_task = job_get_ja_task_template_hold(job,
                                                      first_id, hold_state[i]);
               lList *n_h_ids = nullptr;
               lList *u_h_ids = nullptr;
               lList *o_h_ids = nullptr;
               lList *s_h_ids = nullptr;
               lList *a_h_ids = nullptr;

               ocs::tom::exchange<JB_ja_n_h_ids>(job, &n_h_ids);
               ocs::tom::exchange<JB_ja_u_h_ids>(job, &u_h_ids);
               ocs::tom::exchange<JB_ja_o_h_ids>(job, &o_h_ids);
               ocs::tom::exchange<JB_ja_s_h_ids>(job, &s_h_ids);
               ocs::tom::exchange<JB_ja_a_h_ids>(job, &a_h_ids);

               if (*count == 0) {
                  view.report_pending_jobs_started(os, parameter);
               }
               process_job(os, job, ja_task, nullptr, nullptr, print_jobid, master, &ja_task_id_string, slots, slot, 0, parameter, model, view);
               ocs::tom::exchange<JB_ja_n_h_ids>(job, &n_h_ids);
               ocs::tom::exchange<JB_ja_u_h_ids>(job, &u_h_ids);
               ocs::tom::exchange<JB_ja_o_h_ids>(job, &o_h_ids);
               ocs::tom::exchange<JB_ja_s_h_ids>(job, &s_h_ids);
               ocs::tom::exchange<JB_ja_a_h_ids>(job, &a_h_ids);
               (*count)++;
            }
         } else {
            for_each_ep_lv(range, range_list[i]) {
               uint32_t start, end, step;
               range_get_all_ids(range, &start, &end, &step);
               for (; start <= end; start += step) {
                  lListElem *ja_task = job_get_ja_task_template_hold(job, start, hold_state[i]);
                  sge_dstring_sprintf(&ja_task_id_string, sge_u32, start);

                  if (*count == 0) {
                     view.report_pending_jobs_started(os, parameter);
                  }
                  process_job(os, job, ja_task, nullptr, nullptr, print_jobid, nullptr, &ja_task_id_string, slots, slot, 0, parameter, model, view);
                  (*count)++;
               }
            }
         }
      }
   }

   job_destroy_hold_id_lists(job, range_list);
   sge_dstring_free(&ja_task_id_string);
   DRETURN_VOID;
}

void
ocs::QStatDefaultController::process_queue(std::ostream &os, lListElem *queue, QStatParameter &parameter, QStatModelBase &model, QStatDefaultViewBase &view) {
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
                                                 model.get_job_load_adjustments(), &summary.has_load_value_from_object) ? true : false;

   char load_alarm_reason[MAX_STRING_SIZE] {};
   if (sge_load_alarm(nullptr, 0, queue, ocs::tom::get<QU_load_thresholds>(queue), model.get_exechost_list(), model.get_centry_list(),
                      nullptr, model.get_job_load_adjustments(), true)) {
      qinstance_state_set_alarm(queue, true);

      sge_load_alarm_reason(queue, ocs::tom::get<QU_load_thresholds>(queue), model.get_exechost_list(), model.get_centry_list(),
                            model.get_job_load_adjustments(), load_alarm_reason, MAX_STRING_SIZE - 1, "load");
   }

   char suspend_alarm_reason[MAX_STRING_SIZE] {};
   uint32_t interval;
   parse_ulong_val(nullptr, &interval, CEntry::Type::TIME, ocs::tom::get<QU_suspend_interval>(queue), nullptr, 0);
   if (ocs::tom::get<QU_nsuspend>(queue) != 0 && interval != 0 &&
       sge_load_alarm(nullptr, 0, queue, ocs::tom::get<QU_suspend_thresholds>(queue), model.get_exechost_list(),
                      model.get_centry_list(), nullptr, model.get_job_load_adjustments(), false)) {
      qinstance_state_set_suspend_alarm(queue, true);
      sge_load_alarm_reason(queue, ocs::tom::get<QU_suspend_thresholds>(queue), model.get_exechost_list(), model.get_centry_list(),
                            model.get_job_load_adjustments(), suspend_alarm_reason, MAX_STRING_SIZE - 1, "suspend");
   }

   DSTRING_STATIC(type_string, 32);
   qinstance_print_qtype_to_dstring(queue, &type_string, true);
   summary.queue_type = sge_dstring_get_string(&type_string);
   summary.resv_slots = qinstance_slots_reserved_now(queue);
   summary.used_slots = qinstance_slots_used(queue);
   summary.total_slots = (int)ocs::tom::get<QU_job_slots>(queue);

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
   const char *queue_name = ocs::tom::get<QU_full_name>(queue);
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
      for (ocs::tom::ConstElement<ocs::tom::QIM> qim : ocs::tom::get<QU_message_list>(queue)) {
         uint32_t type = ocs::tom::get<QIM_type>(qim);

         if ((parameter.explain_bits_ & QI_AMBIGUOUS) == type || (parameter.explain_bits_ & QI_ERROR) == type) {
            const char *message = ocs::tom::get<QIM_message>(qim);

            view.report_queue_message(os, queue_name, message);
         }
      }
   }

   /* view (selected) resources of queue in case of -F [attr,attr,..] */
   if (((parameter.show_ & QSTAT_DISPLAY_QRESOURCES))) {
      dstring resource_string = DSTRING_INIT;

      lList *rlp = nullptr;
      queue_complexes2scheduler(&rlp, queue, model.get_exechost_list(), model.get_centry_list(), model.get_job_load_adjustments());

      bool in_resource_list = false;
      for_each_rw_lv(rep , rlp) {
         /* we had a -F request */
         if (parameter.get_q_resource_list()) {
            const lListElem *qres = ocs::tom::find_first<CE_name>(parameter.get_q_resource_list(), ocs::tom::get<CE_name>(rep));
            if (qres == nullptr) {
               qres = ocs::tom::find_first<CE_name>(parameter.get_q_resource_list(), ocs::tom::get<CE_shortcut>(rep));
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
         if (strcmp(ocs::tom::get<CE_name>(rep), LOAD_ATTR_TOPOLOGY) == 0) {
            const char *hostname = ocs::tom::get<QU_qhostname>(queue);
            const lListElem *host = ocs::tom::find_first<EH_name>(model.get_exechost_list(), hostname);
            details = host_get_topology_in_use(host);
         }

         if (!in_resource_list) {
            view.report_queue_resource_started(os, "");
            in_resource_list = true;
         }
         view.report_queue_resource(os, rep, dom, ocs::tom::get<CE_name>(rep), s, details.c_str());
      }

      if (in_resource_list) {
         view.report_queue_resource_finished(os, "");
      }

      lFreeList(&rlp);
      sge_dstring_free(&resource_string);

   }

   DRETURN_VOID;
}

/** @brief Run the request
 * @param parameter the parsed parameters
 * @param model the model, client or server side
 * @param view the view for the requested output format
 */
void ocs::QStatDefaultController::process_request(QStatParameter &parameter, QStatModelBase &model, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);

   model.calc_longest_queue_length(parameter);

   correct_capacities(model.get_exechost_list(), model.get_centry_list(), model.get_job_load_adjustments());

   view.report_started(out_, parameter);
   view.report_queue_section_started(out_, parameter);

   process_queues_with_its_jobs(out_, parameter, model, view);

   remove_tagged_jobs(model.get_job_list());

   // sort pending jobs
   if (lGetNumberOfElem(model.get_job_list()) > 0 ) {
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
