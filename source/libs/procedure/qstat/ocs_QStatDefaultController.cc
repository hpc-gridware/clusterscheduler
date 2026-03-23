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
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_usage.h"

#include "sched/load_correction.h"
#include "sched/sge_complex_schedd.h"
#include "sched/sge_job_schedd.h"
#include "sched/sge_select_queue.h"
#include "sched/sge_urgency.h"

#include "ocs_QStatDefaultController.h"
#include "ocs_client_cqueue.h"

void ocs::QStatDefaultController::remove_tagged_jobs(lList *job_list) {

   lListElem *jep = lFirstRW(job_list);
   lListElem *tmp = nullptr;

   while (jep) {
      lList *task_list;
      lListElem *jatep, *tmp_jatep;

      tmp = lNextRW(jep);
      task_list = lGetListRW(jep, JB_ja_tasks);
      jatep = lFirstRW(task_list);
      while (jatep) {
         tmp_jatep = lNextRW(jatep);
         if ((lGetUlong(jatep, JAT_suitable) & TAG_FOUND_IT)) {
            lRemoveElem(task_list, &jatep);
         }
         jatep = tmp_jatep;
      }
      jep = tmp;
   }

}

void
ocs::QStatDefaultController::process_queues_with_its_jobs(std::ostream &os, QStatParameter &parameter, QStatGenericModel &model, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);

   /* no need to iterate through queues if queues are not printed */
   if (!parameter.need_queues_) {
      process_jobs_in_queue(os, nullptr, 1, parameter, model, view);
      DRETURN_VOID;
   }

   // handle running jobs of a queue
   lListElem *qep = nullptr;
   for_each_rw (qep, model.queue_list) {

      const char* queue_name = lGetString(qep, QU_full_name);

      // here we have the queue
      if (lGetUlong(qep, QU_tag) & TAG_SHOW_IT) {


         if ((parameter.full_listing_ & QSTAT_DISPLAY_NOEMPTYQ) && !qinstance_slots_used(qep)) {
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
   const lListElem *ce, *centry;
   const char *s, *name;
   double uc;

   if (cel == nullptr || lGetNumberOfElem(cel) == 0) {
       DRETURN_VOID;
   }

   if (is_hard_resource) {
      view.report_hard_resources_started(os, scope);
   } else {
      view.report_soft_resources_started(os, scope);
   }

   /* walk through complex entries */
   for_each_ep(ce, cel) {
      name = lGetString(ce, CE_name);
      if ((centry = centry_list_locate(centry_list, name))) {
         uc = centry_urgency_contribution(slots, name, lGetDouble(ce, CE_doubleval), centry);
      } else {
         uc = 0.0;
      }

      s = lGetString(ce, CE_stringval);
      if (is_hard_resource) {
         view.report_hard_resource(os, scope, name, s, uc);
      } else {
         view.report_soft_resource(os, scope, name, s, uc);
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
ocs::QStatDefaultController::process_jobs_in_queue(std::ostream &os, lListElem *queue, bool print_jobs_of_queue, QStatParameter &parameter, QStatGenericModel &model, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);
   u_long32 jid = 0, old_jid;
   u_long32 jataskid = 0, old_jataskid;
   const char *qnm = queue ? lGetString(queue, QU_full_name) : nullptr;
   dstring dyn_task_str = DSTRING_INIT;

   view.report_queue_jobs_started(os, qnm);

   lListElem *jlep;
   for_each_rw(jlep, model.job_list) {
      int master, i;

      lListElem *jatep;
      for_each_rw(jatep, lGetList(jlep, JB_ja_tasks)) {
         u_long32 jstate = lGetUlong(jatep, JAT_state);

         if (ISSET(jstate, JSUSPENDED_ON_SUBORDINATE) ||
             ISSET(jstate, JSUSPENDED_ON_SLOTWISE_SUBORDINATE)) {
            lSetUlong(jatep, JAT_state, jstate & ~JRUNNING);
         }

         lListElem *old_gdilep = nullptr;
         lListElem *gdilep;
         for_each_rw(gdilep, lGetList(jatep, JAT_granted_destin_identifier_list)) {

            if (!queue || !strcmp(lGetString(gdilep, JG_qname), qnm)) {
               int slot_adjust = 0;
               int lines_to_print;
               int slots_per_line = 0;
               int slots_in_queue = lGetUlong(gdilep, JG_slots);

               if (!queue)
                  qnm = lGetString(gdilep, JG_qname);

               u_long32 job_tag = lGetUlong(jatep, JAT_suitable);
               job_tag |= TAG_FOUND_IT;
               lSetUlong(jatep, JAT_suitable, job_tag);

               master = !strcmp(qnm,
                     lGetString(lFirst(lGetList(jatep, JAT_granted_destin_identifier_list)), JG_qname));

               if (master) {
                  const char *pe_name;
                  lListElem *pe;
                  if (((pe_name=lGetString(jatep, JAT_granted_pe))) &&
                      ((pe=pe_list_locate(model.pe_list, pe_name))) &&
                      !lGetBool(pe, PE_job_is_first_task))

                      slot_adjust = 1;
               }

               /* job distribution view ? */
               if (!(parameter.group_opt_ & GROUP_NO_PETASK_GROUPS)) {
                  /* no - condensed ouput format */
                  if (!master && !(parameter.full_listing_ & QSTAT_DISPLAY_FULL)) {
                     /* skip all slave outputs except in full display mode */
                     continue;
                  }

                  /* print only on line per job for this queue */
                  lines_to_print = 1;

                  /* always only show the number of job slots represented by the line */
                  if ((parameter.full_listing_ & QSTAT_DISPLAY_FULL)) {
                     slots_per_line = slots_in_queue;
                  } else {
                     slots_per_line = sge_granted_slots(lGetList(jatep, JAT_granted_destin_identifier_list));
                  }

               } else {
                  /* yes */
                  lines_to_print = (int)slots_in_queue+slot_adjust;
                  slots_per_line = 1;
               }

               for (i=0; i<lines_to_print ;i++) {
                  bool print_jobid = false;
                  bool print_it = false;
                  int different;

                  old_jid = jid;
                  jid = lGetUlong(jlep, JB_job_number);
                  old_jataskid = jataskid;
                  jataskid = lGetUlong(jatep, JAT_task_number);

                  different = (jid != old_jid) || (jataskid != old_jataskid) || (gdilep != old_gdilep);
                  old_gdilep = gdilep;

                  if (different) {
                     print_jobid = true;
                  } else {
                     if (!(parameter.full_listing_ & QSTAT_DISPLAY_RUNNING)) {
                        print_jobid = ((master && (i==0)) ? true : false);
                     } else {
                        print_jobid = false;
                     }
                  }

                  if (!lGetNumberOfElem(parameter.user_list_) ||
                     (lGetNumberOfElem(parameter.user_list_) && (lGetUlong(jatep, JAT_suitable)&TAG_SELECT_IT))) {
                     if (print_jobs_of_queue && (job_tag & TAG_SHOW_IT)) {
                        if ((parameter.full_listing_ & QSTAT_DISPLAY_RUNNING) && (lGetUlong(jatep, JAT_state) & JRUNNING) ) {
                           print_it = true;
                        } else if ((parameter.full_listing_ & QSTAT_DISPLAY_SUSPENDED) &&
                           ((lGetUlong(jatep, JAT_state)&JSUSPENDED) || (lGetUlong(jatep, JAT_state)&JSUSPENDED_ON_THRESHOLD) ||
                           (lGetUlong(jatep, JAT_state)&JSUSPENDED_ON_SUBORDINATE) || (lGetUlong(jatep, JAT_state)&JSUSPENDED_ON_SLOTWISE_SUBORDINATE))) {
                           print_it = true;
                        } else if ((parameter.full_listing_ & QSTAT_DISPLAY_USERHOLD) && (lGetUlong(jatep, JAT_hold)&MINUS_H_TGT_USER)) {
                           print_it = true;
                        } else if ((parameter.full_listing_ & QSTAT_DISPLAY_OPERATORHOLD) && (lGetUlong(jatep, JAT_hold)&MINUS_H_TGT_OPERATOR))  {
                           print_it = true;
                        } else if ((parameter.full_listing_ & QSTAT_DISPLAY_SYSTEMHOLD) && (lGetUlong(jatep, JAT_hold)&MINUS_H_TGT_SYSTEM)) {
                           print_it = true;
                        } else if ((parameter.full_listing_ & QSTAT_DISPLAY_JOBARRAYHOLD) && (lGetUlong(jatep, JAT_hold)&MINUS_H_TGT_JA_AD)) {
                           print_it = true;
                        } else {
                           print_it = false;
                        }
                        if (print_it) {
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

   view.report_queue_jobs_finished(os, qnm, parameter);

   sge_dstring_free(&dyn_task_str);
   DRETURN_VOID;
}

void ocs::QStatDefaultController::process_job(std::ostream &os, lListElem *job, lListElem *jatep, lListElem *qep, lListElem *gdil_ep, bool print_jobid,
                    const char *master, dstring *dyn_task_str, int slots, int slot, int slots_per_line,
                    QStatParameter &parameter, QStatGenericModel &model, QStatDefaultViewBase &view)
{
   DENTER(TOP_LAYER);
   u_long32 jstate;
   int sge_ext, tsk_ext, sge_urg, sge_pri, sge_time;
   const lList *ql = nullptr;
   const lListElem *qrep;
   QStatDefaultViewBase::job_summary_t summary{};

   summary.print_jobid = print_jobid;

   if (gdil_ep) {
      summary.queue = lGetString(gdil_ep, JG_qname);
   }

   sge_ext = ((parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED) == QSTAT_DISPLAY_EXTENDED);
   tsk_ext = (parameter.full_listing_ & QSTAT_DISPLAY_TASKS);
   sge_urg = (parameter.full_listing_ & QSTAT_DISPLAY_URGENCY);
   sge_pri = (parameter.full_listing_ & QSTAT_DISPLAY_PRIORITY);
   sge_time = !sge_ext;
   sge_time = sge_time | tsk_ext | sge_urg | sge_pri;

   summary.nprior = lGetDouble(jatep, JAT_prio);
   if (sge_pri || sge_urg) {
      summary.nurg = lGetDouble(job, JB_nurg);
   }
   if (sge_pri) {
      summary.nppri = lGetDouble(job, JB_nppri);
   }
   if (sge_pri || sge_ext) {
      summary.ntckts =  lGetDouble(jatep, JAT_ntix);
   }
   if (sge_urg) {
      summary.urg = lGetDouble(job, JB_urg);
      summary.rrcontr = lGetDouble(job, JB_rrcontr);
      summary.wtcontr = lGetDouble(job, JB_wtcontr);
      summary.dlcontr = lGetDouble(job, JB_dlcontr);
   }
   if (sge_pri) {
      summary.priority = static_cast<int>(lGetUlong(job, JB_priority)) - BASE_PRIORITY;
   }
   summary.name = lGetString(job, JB_job_name);
   summary.user = lGetString(job, JB_owner);
   if (sge_ext) {
      summary.project = lGetString(job, JB_project);
      summary.department = lGetString(job, JB_department);
   }

   /* move status info into state info */
   jstate = lGetUlong(jatep, JAT_state);
   if (lGetUlong(jatep, JAT_status)==JTRANSFERING) {
      jstate |= JTRANSFERING;
      jstate &= ~JRUNNING;
   }

   if (lGetList(job, JB_jid_predecessor_list) || lGetUlong(jatep, JAT_hold)) {
      jstate |= JHELD;
   }

   if (lGetUlong(jatep, JAT_job_restarted)) {
      jstate &= ~JWAITING;
      jstate |= JMIGRATING;
   }

   job_get_state_string(summary.state, jstate);
   if (sge_time) {
      summary.submit_time = lGetUlong64(job, JB_submission_time);
      summary.start_time = lGetUlong64(jatep, JAT_start_time);
   }

   if (lGetUlong(jatep, JAT_status)==JRUNNING || lGetUlong(jatep, JAT_status)==JTRANSFERING) {
      summary.is_running = true;
   } else {
      summary.is_running = false;
   }

   if (sge_urg) {
      summary.deadline = lGetUlong64(job, JB_deadline);
   }

   if (sge_ext) {
      const lListElem *up, *pe, *task;
      lList *job_usage_list;
      const char *pe_name;
      bool sum_pe_tasks = false;

      if (master == nullptr || strcmp(master, "MASTER") == 0) {
         if (!(parameter.group_opt_ & GROUP_NO_PETASK_GROUPS)) {
            sum_pe_tasks = true;
         }
         job_usage_list = lCopyList(nullptr, lGetList(jatep, JAT_scaled_usage_list));
      } else {
         job_usage_list = lCreateList("", UA_Type);
      }

      /* sum pe-task usage based on queue slots */
      if (job_usage_list) {
         int subtask_ndx=1;
         for_each_ep(task, lGetList(jatep, JAT_task_list)) {
            lListElem *dst;
            const lListElem *src;
            const lListElem *ep;
            const char *qname;

            if (sum_pe_tasks ||
                (summary.queue &&
                 ((ep=lFirst(lGetList(task, PET_granted_destin_identifier_list)))) &&
                 ((qname=lGetString(ep, JG_qname))) &&
                 !strcmp(qname, summary.queue) && ((subtask_ndx++%slots)==slot))) {
               for_each_ep(src, lGetList(task, PET_scaled_usage)) {
                  if ((dst=lGetElemStrRW(job_usage_list, UA_name, lGetString(src, UA_name))))
                     lSetDouble(dst, UA_value, lGetDouble(dst, UA_value) + lGetDouble(src, UA_value));
                  else
                     lAppendElem(job_usage_list, lCopyElem(src));
               }
            }
         }
      }


      /* scaled cpu usage */
      if (!(up = lGetElemStr(job_usage_list, UA_name, USAGE_ATTR_CPU))) {
         summary.has_cpu_usage = false;
      } else {
         summary.has_cpu_usage = true;
         summary.cpu_usage = lGetDouble(up, UA_value);
      }
      /* scaled mem usage */
      if (!(up = lGetElemStr(job_usage_list, UA_name, USAGE_ATTR_MEM))) {
         summary.has_mem_usage = false;
      } else {
         summary.has_mem_usage = true;
         summary.mem_usage = lGetDouble(up, UA_value);
      }
      /* scaled io usage */
      if (!(up = lGetElemStr(job_usage_list, UA_name, USAGE_ATTR_IO))) {
         summary.has_io_usage = false;
      } else {
         summary.has_io_usage = true;
         summary.io_usage = lGetDouble(up, UA_value);
      }
      lFreeList(&job_usage_list);

      /* get tickets for job/slot */

      summary.override_tickets = lGetUlong(job, JB_override_tickets);
      summary.share = lGetDouble(jatep, JAT_share);
      if (sge_ext) {
         summary.is_queue_assigned = false;
      } else {
         if (lGetList(jatep, JAT_granted_destin_identifier_list) != nullptr) {
            summary.is_queue_assigned = true;
         } else {
            summary.is_queue_assigned = false;
         }
      }

      /* braces needed to suppress compiler warnings */
      if ((pe_name=lGetString(jatep, JAT_granted_pe)) &&
           (pe=pe_list_locate(model.pe_list, pe_name)) &&
           lGetBool(pe, PE_control_slaves) && slots) {
         if (slot == 0) {
            summary.tickets = (u_long)lGetDouble(gdil_ep, JG_ticket);
            summary.otickets = (u_long)lGetDouble(gdil_ep, JG_oticket);
            summary.ftickets = (u_long)lGetDouble(gdil_ep, JG_fticket);
            summary.stickets = (u_long)lGetDouble(gdil_ep, JG_sticket);
         }
         else {
            if (slots) {
               summary.tickets = (u_long)(lGetDouble(gdil_ep, JG_ticket) / slots);
               summary.otickets = (u_long)(lGetDouble(gdil_ep, JG_oticket) / slots);
               summary.ftickets = (u_long)(lGetDouble(gdil_ep, JG_fticket) / slots);
               summary.stickets = (u_long)(lGetDouble(gdil_ep, JG_sticket) / slots);
            }
            else {
               summary.tickets = summary.otickets = summary.ftickets = summary.stickets = 0;
            }
         }
      }
      else {
         summary.tickets = (u_long)lGetDouble(jatep, JAT_tix);
         summary.otickets = (u_long)lGetDouble(jatep, JAT_oticket);
         summary.ftickets = (u_long)lGetDouble(jatep, JAT_fticket);
         summary.stickets = (u_long)lGetDouble(jatep, JAT_sticket);
      }

   }

   summary.master = master;
   if (slots_per_line == 0) {
      summary.slots = sge_job_slot_request(job, model.pe_list);
   } else {
      summary.slots = slots_per_line;
   }

   summary.is_array = job_is_array(job);

   if (summary.is_array) {
      summary.task_id = sge_dstring_get_string(dyn_task_str);
   } else {
      summary.task_id = nullptr;
   }

   view.report_job(os, lGetUlong(job, JB_job_number), &summary, parameter, model);

   if (tsk_ext) {
      const lList *task_list = lGetList(jatep, JAT_task_list);
      lListElem *task;
      const lListElem *ep;
      const char *qname;
      int subtask_ndx=1;

      view.report_sub_tasks_started(os);

      /* print master sub-task belonging to this queue */
      if (!slot && task_list && summary.queue &&
          ((ep=lFirst(lGetList(jatep, JAT_granted_destin_identifier_list)))) &&
          ((qname=lGetString(ep, JG_qname))) &&
          !strcmp(qname, summary.queue)) {

          process_subtask(os, job, jatep, nullptr, view);
      }

      /* print sub-tasks belonging to this queue */
      for_each_rw (task, task_list) {
         if (!slots || (summary.queue &&
              ((ep=lFirst(lGetList(task, PET_granted_destin_identifier_list)))) &&
              ((qname=lGetString(ep, JG_qname))) &&
              !strcmp(qname, summary.queue) && ((subtask_ndx++%slots)==slot))) {
             process_subtask(os, job, jatep, task, view);
         }
      }

      view.report_sub_tasks_finished(os);
   }

   /* print additional job info if requested */
   if ((parameter.full_listing_ & QSTAT_DISPLAY_RESOURCES)) {

      view.report_additional_info(os, QStatDefaultViewBase::FULL_JOB_NAME, lGetString(job, JB_job_name));
      if (summary.queue) {
         view.report_additional_info(os, QStatDefaultViewBase::MASTER_QUEUE, summary.queue);
      }

      if (lGetString(job, JB_pe)) {
         dstring range_string = DSTRING_INIT;

         range_list_print_to_string(lGetList(job, JB_pe_range),
                                    &range_string, true, false, false);

         view.report_requested_pe(os, lGetString(job, JB_pe), sge_dstring_get_string(&range_string));

         sge_dstring_free(&range_string);
      }

      if (lGetString(jatep, JAT_granted_pe)) {
         u_long32 pe_slots = 0;
         const lListElem *l_gdil_ep;
         for_each_ep(l_gdil_ep, lGetList(jatep, JAT_granted_destin_identifier_list)) {
            pe_slots += lGetUlong(l_gdil_ep, JG_slots);
         }

         view.report_granted_pe(os, lGetString(jatep, JAT_granted_pe), pe_slots);
      }
      if (lGetString(job, JB_checkpoint_name)) {
         view.report_additional_info(os, QStatDefaultViewBase::CHECKPOINT_ENV, lGetString(job, JB_checkpoint_name));
      }

      /* Handle the Hard Resources (global, master, slave) */
      process_resources(os, job_get_hard_resource_list(job, JRS_SCOPE_GLOBAL), model.centry_list,
                           sge_job_slot_request(job, model.pe_list), JRS_SCOPE_GLOBAL, true, view);

      process_resources(os, job_get_hard_resource_list(job, JRS_SCOPE_MASTER), model.centry_list,
                           sge_job_slot_request(job, model.pe_list), JRS_SCOPE_MASTER, true, view);

      process_resources(os, job_get_hard_resource_list(job, JRS_SCOPE_SLAVE), model.centry_list,
                           sge_job_slot_request(job, model.pe_list),
                           JRS_SCOPE_SLAVE, true, view);

      /* display default requests if necessary */
      {
         lList *attributes = nullptr;
         const lListElem *ce;
         const char *name;
         lListElem *hep;

         queue_complexes2scheduler(&attributes, qep, model.exechost_list, model.centry_list);
         for_each_ep(ce, attributes) {
            double dval;

            name = lGetString(ce, CE_name);
            if (!lGetUlong(ce, CE_consumable) || !strcmp(name, "slots") ||
                job_get_request(job, name)) {
               continue;
            }

            parse_ulong_val(&dval, nullptr, lGetUlong(ce, CE_valtype), lGetString(ce, CE_defaultval), nullptr, 0);
            if (dval == 0.0) {
               continue;
            }

            /* For pending jobs (no queue/no exec host) we may print default request only
               if the consumable is specified in the global host. For running we print it
               if the resource is managed at this node/queue */
            if ((qep && lGetSubStr(qep, CE_name, name, QU_consumable_config_list)) ||
                (qep && (hep=host_list_locate(model.exechost_list, lGetHost(qep, QU_qhostname))) &&
                 lGetSubStr(hep, CE_name, name, EH_consumable_config_list)) ||
                  ((hep=host_list_locate(model.exechost_list, SGE_GLOBAL_NAME)) &&
                  lGetSubStr(hep, CE_name, name, EH_consumable_config_list))) {

                     view.report_request(os, name, lGetString(ce, CE_defaultval));
            }
         }
         lFreeList(&attributes);
      }

      /* Handle the Soft Resources */
      process_resources(os, job_get_soft_resource_list(job), model.centry_list,
                           sge_job_slot_request(job, model.pe_list), JRS_SCOPE_GLOBAL, false, view);

      {
         ql = job_get_hard_queue_list(job, JRS_SCOPE_GLOBAL);
         if (ql != nullptr && lGetNumberOfElem(ql) != 0) {
            view.report_hard_requested_queues_started(os, JRS_SCOPE_GLOBAL);
            for_each_ep(qrep, ql) {
               view.report_hard_requested_queue(os, JRS_SCOPE_GLOBAL, lGetString(qrep, QR_name));
            }
            view.report_hard_requested_queues_finished(os);
         }
         ql = job_get_hard_queue_list(job, JRS_SCOPE_MASTER);
         if (ql != nullptr && lGetNumberOfElem(ql) != 0) {
            view.report_hard_requested_queues_started(os, JRS_SCOPE_MASTER);
            for_each_ep(qrep, ql) {
               view.report_hard_requested_queue(os, JRS_SCOPE_MASTER, lGetString(qrep, QR_name));
            }
            view.report_hard_requested_queues_finished(os);
         }
         ql = job_get_hard_queue_list(job, JRS_SCOPE_SLAVE);
         if (ql != nullptr && lGetNumberOfElem(ql) != 0) {
            view.report_hard_requested_queues_started(os, JRS_SCOPE_SLAVE);
            for_each_ep(qrep, ql) {
               view.report_hard_requested_queue(os, JRS_SCOPE_SLAVE, lGetString(qrep, QR_name));
            }
            view.report_hard_requested_queues_finished(os);
         }
      }

      {
         ql = job_get_soft_queue_list(job, JRS_SCOPE_GLOBAL);
         if (ql != nullptr && lGetNumberOfElem(ql) != 0) {
            view.report_soft_requested_queues_started(os,JRS_SCOPE_GLOBAL);
            for_each_ep(qrep, ql) {
               view.report_soft_requested_queue(os, JRS_SCOPE_GLOBAL, lGetString(qrep, QR_name));
            }
            view.report_soft_requested_queues_finished(os);
         }
         ql = job_get_soft_queue_list(job, JRS_SCOPE_MASTER);
         if (ql != nullptr && lGetNumberOfElem(ql) != 0) {
            view.report_soft_requested_queues_started(os, JRS_SCOPE_MASTER);
            for_each_ep(qrep, ql) {
               view.report_soft_requested_queue(os, JRS_SCOPE_MASTER, lGetString(qrep, QR_name));
            }
            view.report_soft_requested_queues_finished(os);
         }
         ql = job_get_soft_queue_list(job, JRS_SCOPE_SLAVE);
         if (ql != nullptr && lGetNumberOfElem(ql) != 0) {
            view.report_soft_requested_queues_started(os, JRS_SCOPE_SLAVE);
            for_each_ep(qrep, ql) {
               view.report_soft_requested_queue(os, JRS_SCOPE_SLAVE, lGetString(qrep, QR_name));
            }
            view.report_soft_requested_queues_finished(os);
         }
      }

      {
         ql = lGetList(job, JB_jid_request_list );
         if (ql) {
            view.report_predecessors_requested_started(os);
            for_each_ep(qrep, ql) {
               view.report_predecessor_requested(os, lGetString(qrep, JRE_job_name));
            }
            view.report_predecessors_requested_finished(os);
         }
      }
      {
         ql = lGetList(job, JB_jid_predecessor_list);
         if (ql) {
            view.report_predecessors_started(os);
            for_each_ep(qrep, ql) {
               view.report_predecessor(os, lGetUlong(qrep, JRE_job_number));
            }
            view.report_predecessors_finished(os);
         }
      }

      {
         ql = lGetList(job, JB_ja_ad_request_list );
         if (ql) {
            view.report_ad_predecessors_requested_started(os);
            for_each_ep(qrep, ql) {
               view.report_ad_predecessor_requested(os, lGetString(qrep, JRE_job_name));
            }
            view.report_ad_predecessors_requested_finished(os);
         }
      }
      {
         ql = lGetList(job, JB_ja_ad_predecessor_list);
         if (ql) {
            view.report_ad_predecessors_started(os);
            for_each_ep(qrep, ql) {
               view.report_ad_predecessor(os, lGetUlong(qrep, JRE_job_number));
            }
            view.report_ad_predecessors_finished(os);
         }
      }

      if (const lListElem *binding_elem = lGetObject(job, JB_binding); binding_elem != nullptr) {
         dstring binding_param = DSTRING_INIT;

         std::string binding_str;
         BindingIo::binding_print_to_string(binding_elem, binding_str);
         sge_dstring_sprintf(&binding_param, "%s", binding_str.c_str());

         view.report_binding_started(os);
         view.report_binding(os, sge_dstring_get_string(&binding_param));
         view.report_binding_finished(os);
         sge_dstring_free(&binding_param);
      }
   }

   view.report_job_finished(os, lGetUlong(job, JB_job_number));

#undef QSTAT_INDENT
#undef QSTAT_INDENT2

   DRETURN_VOID;
}

void
ocs::QStatDefaultController::process_subtask(std::ostream &os, lListElem *job, lListElem *ja_task, lListElem *pe_task, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);
   char task_state_string[8];
   u_long32 tstate, tstatus;
   const lListElem *ep;
   const lList *usage_list;
   const lList *scaled_usage_list;

   QStatDefaultViewBase::task_summary_t summary{};

   /* is sub-task logically running */
   if (pe_task == nullptr) {
      tstatus = lGetUlong(ja_task, JAT_status);
      usage_list = lGetList(ja_task, JAT_usage_list);
      scaled_usage_list = lGetList(ja_task, JAT_scaled_usage_list);
   } else {
      tstatus = lGetUlong(pe_task, PET_status);
      usage_list = lGetList(pe_task, PET_usage);
      scaled_usage_list = lGetList(pe_task, PET_scaled_usage);
   }

   if (pe_task == nullptr) {
      summary.task_id = "";
   } else {
      summary.task_id = lGetString(pe_task, PET_id);
   }

   /* move status info into state info */
   tstate = lGetUlong(ja_task, JAT_state);
   if (tstatus==JRUNNING) {
      tstate |= JRUNNING;
      tstate &= ~JTRANSFERING;
   } else if (tstatus==JTRANSFERING) {
      tstate |= JTRANSFERING;
      tstate &= ~JRUNNING;
   } else if (tstatus==JFINISHED) {
      tstate |= JEXITING;
      tstate &= ~(JRUNNING|JTRANSFERING);
   }

   if (lGetList(job, JB_jid_predecessor_list) || lGetUlong(ja_task, JAT_hold)) {
      tstate |= JHELD;
   }

   if (lGetUlong(ja_task, JAT_job_restarted)) {
      tstate &= ~JWAITING;
      tstate |= JMIGRATING;
   }

   /* write states into string */
   job_get_state_string(task_state_string, tstate);
   summary.state = task_state_string;

   {
      const lListElem *up;

      /* scaled cpu usage */
      if (!(up = lGetElemStr(scaled_usage_list, UA_name, USAGE_ATTR_CPU))) {
         summary.has_cpu_usage = false;
      } else {
         summary.has_cpu_usage = true;
         summary.cpu_usage = lGetDouble(up, UA_value);
      }

      /* scaled mem usage */
      if (!(up = lGetElemStr(scaled_usage_list, UA_name, USAGE_ATTR_MEM))) {
         summary.has_mem_usage = false;
      } else {
         summary.has_mem_usage = true;
         summary.mem_usage = lGetDouble(up, UA_value);
      }

      /* scaled io usage */
      if (!(up = lGetElemStr(scaled_usage_list, UA_name, USAGE_ATTR_IO))) {
         summary.has_io_usage = false;
      } else {
         summary.has_io_usage = true;
         summary.io_usage = lGetDouble(up, UA_value);
      }
   }

   if (tstatus==JFINISHED) {
      ep=lGetElemStr(usage_list, UA_name, "exit_status");
      if (ep) {
         summary.has_exit_status = true;
         summary.exit_status = lGetDouble(ep, UA_value);
      } else {
         summary.has_exit_status = false;
      }
   } else {
      summary.has_exit_status = false;
   }

   view.report_sub_task(os, &summary);

   DRETURN_VOID;
}

void ocs::QStatDefaultController::process_jobs_pending_state(std::ostream &os, ocs::QStatParameter &parameter, ocs::QStatGenericModel &model, ocs::QStatDefaultViewBase &view) {

   lListElem *nxt, *jep, *jatep, *nxt_jatep;
   lList* ja_task_list = nullptr;
   int FoundTasks;
   int count = 0;
   dstring dyn_task_str = DSTRING_INIT;

   DENTER(TOP_LAYER);

   nxt = lFirstRW(model.job_list);
   while ((jep=nxt)) {
      nxt = lNextRW(jep);
      nxt_jatep = lFirstRW(lGetList(jep, JB_ja_tasks));
      FoundTasks = 0;

      bool hide_data = !job_is_visible(lGetString(jep, JB_owner), model.is_manager_);
      if (hide_data) {
         continue;
      }

      while ((jatep = nxt_jatep)) {
         nxt_jatep = lNextRW(jatep);

         if (!(((parameter.full_listing_ & QSTAT_DISPLAY_OPERATORHOLD) && (lGetUlong(jatep, JAT_hold)&MINUS_H_TGT_OPERATOR))
               ||
             ((parameter.full_listing_ & QSTAT_DISPLAY_USERHOLD) && (lGetUlong(jatep, JAT_hold)&MINUS_H_TGT_USER))
               ||
             ((parameter.full_listing_ & QSTAT_DISPLAY_SYSTEMHOLD) && (lGetUlong(jatep, JAT_hold)&MINUS_H_TGT_SYSTEM))
               ||
             ((parameter.full_listing_ & QSTAT_DISPLAY_JOBARRAYHOLD) && (lGetUlong(jatep, JAT_hold)&MINUS_H_TGT_JA_AD))
               ||
             ((parameter.full_listing_ & QSTAT_DISPLAY_JOBHOLD) && lGetList(jep, JB_jid_predecessor_list))
               ||
             ((parameter.full_listing_ & QSTAT_DISPLAY_STARTTIMEHOLD) && lGetUlong64(jep, JB_execution_time))
               ||
             !(parameter.full_listing_ & QSTAT_DISPLAY_HOLD))
            ) {
            break;
         }

         if (!(lGetUlong(jatep, JAT_suitable) & TAG_FOUND_IT) &&
            VALID(JQUEUED, lGetUlong(jatep, JAT_state)) &&
            !VALID(JFINISHED, lGetUlong(jatep, JAT_status))) {
            lSetUlong(jatep, JAT_suitable,
            lGetUlong(jatep, JAT_suitable)|TAG_FOUND_IT);

            if ((!lGetNumberOfElem(parameter.user_list_) || (lGetNumberOfElem(parameter.user_list_) && (lGetUlong(jatep, JAT_suitable)&TAG_SELECT_IT))) &&
                (lGetUlong(jatep, JAT_suitable)&TAG_SHOW_IT)) {

               if ((parameter.full_listing_ & QSTAT_DISPLAY_PENDING) &&
                   (parameter.group_opt_ & GROUP_NO_TASK_GROUPS) > 0) {

                  sge_dstring_sprintf(&dyn_task_str, sge_u32, lGetUlong(jatep, JAT_task_number));

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
      if ((parameter.full_listing_ & QSTAT_DISPLAY_PENDING)  &&
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
      if (jep != nxt && (parameter.full_listing_ & QSTAT_DISPLAY_PENDING)) {
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


void ocs::QStatDefaultController::process_jobs_finished_state(std::ostream &os, ocs::QStatParameter &parameter, ocs::QStatGenericModel &model, ocs::QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);
   lListElem *jep, *jatep;
   int count = 0;
   dstring dyn_task_str = DSTRING_INIT;

   for_each_rw (jep, model.job_list) {
      for_each_rw (jatep, lGetList(jep, JB_ja_tasks)) {
         if (lGetUlong(jatep, JAT_status) == JFINISHED) {
            lSetUlong(jatep, JAT_suitable, lGetUlong(jatep, JAT_suitable)|TAG_FOUND_IT);

            if (!getenv("MORE_INFO"))
               continue;

            if (!lGetNumberOfElem(parameter.user_list_) || (lGetNumberOfElem(parameter.user_list_) &&
                  (lGetUlong(jatep, JAT_suitable)&TAG_SELECT_IT))) {

               if (count == 0) {
                  view.report_finished_jobs_started(os, parameter);
               }
               sge_dstring_sprintf(&dyn_task_str, sge_u32, lGetUlong(jatep, JAT_task_number));

               process_job(os, jep, jatep, nullptr, nullptr, true, nullptr, &dyn_task_str, 0, 0, 0, parameter, model, view);

               count++;
            }
         }
      }
   }

   if (count > 0) {
      view.report_finished_jobs_finished(os);
   }

   sge_dstring_free(&dyn_task_str);
   DRETURN_VOID;
}


void ocs::QStatDefaultController::process_jobs_error_state(std::ostream &os, QStatParameter &parameter, QStatGenericModel &model, QStatDefaultViewBase &view) {
   lListElem *jep, *jatep;
   int count = 0;
   dstring dyn_task_str = DSTRING_INIT;

   DENTER(TOP_LAYER);

   for_each_rw (jep, model.job_list) {
      for_each_rw (jatep, lGetList(jep, JB_ja_tasks)) {
         if (!(lGetUlong(jatep, JAT_suitable) & TAG_FOUND_IT) && lGetUlong(jatep, JAT_status) == JERROR) {
            lSetUlong(jatep, JAT_suitable, lGetUlong(jatep, JAT_suitable)|TAG_FOUND_IT);

            if (!lGetNumberOfElem(parameter.user_list_) || (lGetNumberOfElem(parameter.user_list_) && (lGetUlong(jatep, JAT_suitable)&TAG_SELECT_IT))) {
               sge_dstring_sprintf(&dyn_task_str, sge_u32, lGetUlong(jatep, JAT_task_number));

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
                                    QStatParameter &parameter, QStatGenericModel &model, QStatDefaultViewBase &view)
{
   DENTER(TOP_LAYER);
   lList *range_list[16];         /* RN_Type */
   u_long32 hold_state[16];
   int i;
   dstring ja_task_id_string = DSTRING_INIT;


   job_create_hold_id_lists(job, range_list, hold_state);
   for (i = 0; i <= 15; i++) {
      lList *answer_list = nullptr;
      u_long32 first_id;
      int show = 0;

      if (((parameter.full_listing_ & QSTAT_DISPLAY_USERHOLD) && (hold_state[i] & MINUS_H_TGT_USER)) ||
          ((parameter.full_listing_ & QSTAT_DISPLAY_OPERATORHOLD) && (hold_state[i] & MINUS_H_TGT_OPERATOR)) ||
          ((parameter.full_listing_ & QSTAT_DISPLAY_SYSTEMHOLD) && (hold_state[i] & MINUS_H_TGT_SYSTEM)) ||
          ((parameter.full_listing_ & QSTAT_DISPLAY_JOBARRAYHOLD) && (hold_state[i] & MINUS_H_TGT_JA_AD)) ||
          ((parameter.full_listing_ & QSTAT_DISPLAY_STARTTIMEHOLD) && (lGetUlong64(job, JB_execution_time) > 0)) ||
          ((parameter.full_listing_ & QSTAT_DISPLAY_JOBHOLD) && (lGetList(job, JB_jid_predecessor_list) != 0)) ||
          (!(parameter.full_listing_ & QSTAT_DISPLAY_HOLD))
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

               lXchgList(job, JB_ja_n_h_ids, &n_h_ids);
               lXchgList(job, JB_ja_u_h_ids, &u_h_ids);
               lXchgList(job, JB_ja_o_h_ids, &o_h_ids);
               lXchgList(job, JB_ja_s_h_ids, &s_h_ids);
               lXchgList(job, JB_ja_a_h_ids, &a_h_ids);

               if (*count == 0) {
                  view.report_pending_jobs_started(os, parameter);
               }
               process_job(os, job, ja_task, nullptr, nullptr, print_jobid, master, &ja_task_id_string, slots, slot, 0, parameter, model, view);
               lXchgList(job, JB_ja_n_h_ids, &n_h_ids);
               lXchgList(job, JB_ja_u_h_ids, &u_h_ids);
               lXchgList(job, JB_ja_o_h_ids, &o_h_ids);
               lXchgList(job, JB_ja_s_h_ids, &s_h_ids);
               lXchgList(job, JB_ja_a_h_ids, &a_h_ids);
               (*count)++;
            }
         } else {
            const lListElem *range; /* RN_Type */

            for_each_ep(range, range_list[i]) {
               u_long32 start, end, step;
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
ocs::QStatDefaultController::process_queue(std::ostream &os, lListElem *queue, QStatParameter &parameter, QStatGenericModel &model, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);

   if (!(parameter.full_listing_ & QSTAT_DISPLAY_FULL)) {
      DRETURN_VOID;
   }

   QStatDefaultViewBase::queue_summary_t summary {};
   summary.load_avg_str = LOAD_ATTR_LOAD_AVG;


   // compute the load and check for alarm states
   summary.has_load_value = sge_get_double_qattr(&summary.load_avg, summary.load_avg_str, queue,
                                                 model.exechost_list, model.centry_list,
                                                 &summary.has_load_value_from_object) ? true : false;

   char load_alarm_reason[MAX_STRING_SIZE] {};
   char suspend_alarm_reason[MAX_STRING_SIZE] {};
   if (sge_load_alarm(nullptr, 0, queue, lGetList(queue, QU_load_thresholds), model.exechost_list, model.centry_list, nullptr, true)) {
      qinstance_state_set_alarm(queue, true);
      sge_load_alarm_reason(queue, lGetListRW(queue, QU_load_thresholds), model.exechost_list, model.centry_list, load_alarm_reason, MAX_STRING_SIZE - 1, "load");
   }

   u_long32 interval;
   parse_ulong_val(nullptr, &interval, TYPE_TIM, lGetString(queue, QU_suspend_interval), nullptr, 0);
   if (lGetUlong(queue, QU_nsuspend) != 0 && interval != 0 &&
       sge_load_alarm(nullptr, 0, queue, lGetList(queue, QU_suspend_thresholds), model.exechost_list, model.centry_list, nullptr, false)) {
      qinstance_state_set_suspend_alarm(queue, true);
      sge_load_alarm_reason(queue, lGetListRW(queue, QU_suspend_thresholds), model.exechost_list, model.centry_list, suspend_alarm_reason, MAX_STRING_SIZE - 1, "suspend");
   }

   DSTRING_STATIC(type_string, 32);
   qinstance_print_qtype_to_dstring(queue, &type_string, true);
   summary.queue_type = sge_dstring_get_string(&type_string);
   summary.resv_slots = qinstance_slots_reserved_now(queue);
   summary.used_slots = qinstance_slots_used(queue);
   summary.total_slots = (int)lGetUlong(queue, QU_job_slots);

   /* arch */
   char arch_string[80];
   if (!sge_get_string_qattr(arch_string, sizeof(arch_string)-1, LOAD_ATTR_ARCH, queue, model.exechost_list, model.centry_list)) {
      summary.arch = arch_string;
   } else {
      summary.arch = nullptr;
   }

   DSTRING_STATIC(state_string, 32);
   qinstance_state_append_to_dstring(queue, &state_string);
   summary.state = sge_dstring_get_string(&state_string);

   // does the executing qstat user have access to this queue?
   const char *queue_name = lGetString(queue, QU_full_name);
   view.report_queue_summary(os, queue_name, &summary, parameter);

   if ((parameter.full_listing_ & QSTAT_DISPLAY_ALARMREASON)) {
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
      const lList *qim_list = lGetList(queue, QU_message_list);
      const lListElem *qim = nullptr;

      for_each_ep(qim, qim_list) {
         u_long32 type = lGetUlong(qim, QIM_type);

         if ((parameter.explain_bits_ & QI_AMBIGUOUS) == type || (parameter.explain_bits_ & QI_ERROR) == type) {
            const char *message = lGetString(qim, QIM_message);

            view.report_queue_message(os, queue_name, message);
         }
      }
   }

   /* view (selected) resources of queue in case of -F [attr,attr,..] */
   if (((parameter.full_listing_ & QSTAT_DISPLAY_QRESOURCES))) {
      dstring resource_string = DSTRING_INIT;

      lList *rlp = nullptr;
      queue_complexes2scheduler(&rlp, queue, model.exechost_list, model.centry_list);

      lListElem *rep;
      for_each_rw (rep , rlp) {
         /* we had a -F request */
         if (parameter.qresource_list_) {
            const lListElem *qres = lGetElemStr(parameter.qresource_list_, CE_name, lGetString(rep, CE_name));
            if (qres == nullptr) {
               qres = lGetElemStr(parameter.qresource_list_, CE_name, lGetString(rep, CE_shortcut));
            }

            /* if this complex variable wasn't requested with -F, skip it */
            if (qres == nullptr) {
               continue ;
            }
         }

         sge_dstring_clear(&resource_string);
         char dom[5];
         u_long32 dominant = 0;
         const char *s = sge_get_dominant_stringval(rep, &dominant, &resource_string);
         monitor_dominance(dom, dominant);

         std::string details;
         if (strcmp(lGetString(rep, CE_name), LOAD_ATTR_TOPOLOGY) == 0) {
            const char *hostname = lGetHost(queue, QU_qhostname);
            const lListElem *host = lGetElemHost(model.exechost_list, EH_name, hostname);
            details = host_get_topology_in_use(host);
         }

         view.report_queue_resource(os, dom, lGetString(rep, CE_name), s, details.c_str());
      }

      lFreeList(&rlp);
      sge_dstring_free(&resource_string);

   }

   DRETURN_VOID;
}

void ocs::QStatDefaultController::process_request(QStatParameter &parameter, QStatGenericModel &model, QStatDefaultViewBase &view) {
   DENTER(TOP_LAYER);

   model.calc_longest_queue_length(parameter);

   correct_capacities(model.exechost_list, model.centry_list);

   std::ostringstream oss;

   view.report_started(oss);

   process_queues_with_its_jobs(oss, parameter, model, view);

   remove_tagged_jobs(model.job_list);

   // sort pending jobs
   if (lGetNumberOfElem(model.job_list)>0 ) {
      Job::sgeee_sort_jobs(&model.job_list);
   }

   process_jobs_pending_state(oss, parameter, model, view);
   process_jobs_finished_state(oss, parameter, model, view);
   process_jobs_error_state(oss, parameter, model, view);

   view.report_finished(oss);

   std::cout << oss.str();

   DRETURN_VOID;
}
