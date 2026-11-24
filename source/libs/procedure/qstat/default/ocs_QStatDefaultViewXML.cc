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
 * @brief XML rendering of plain `qstat`
 */

#include <cstdio>
#include <cstdlib>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_log.h"
#include "uti/sge_time.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/tom/ocs_tom_all.h"

#include "sgeobj/parse.h"
#include "sgeobj/ocs_object_xml.h"
#include "sgeobj/sge_job.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_XMLA.h"
#include "sgeobj/tom/ocs_tom_XMLE.h"

#include "qstat/default/ocs_QStatDefaultViewXML.h"

static const char* ADDITIONAL_TAG_NAMES[] = {
   "ERROR",
   "checkpoint_env",
   "master_queue",
   "full_job_name",
   "requested_pe",
   "granted_pe"
};

/** @brief Start a new job section
 * @return none
 */
void ocs::QStatDefaultViewXML::qstat_xml_create_job_list() {
   DENTER(TOP_LAYER);

   SGE_ASSERT(job_list == nullptr);
   job_list = ocs::tom::ListPtr<ocs::tom::XMLE>::create("job_list");
   DRETURN_VOID;
}

/** @brief Close the current job section and file it under its state
 *
 * Each job gets the state as an attribute, and the jobs move to the end of
 * the target list; the section's list is freed.
 *
 * @param state the section's name in the document, e.g. `pending_jobs`
 * @param target_list the list the section is appended to
 * @return none
 */
void ocs::QStatDefaultViewXML::qstat_xml_finish_job_list(const char *state,
                                                         const ocs::tom::List<ocs::tom::XMLE> target_list) {
   DENTER(TOP_LAYER);

   for (const auto l_job_elem : *job_list) {
      const auto attributes = l_job_elem.get_or_create<XMLE_Attribute>("attributes");
      auto state_elem = ocs::tom::ElementPtr<ocs::tom::XMLA>::create();
      state_elem->set<XMLA_Name>("state");
      state_elem->set<XMLA_Value>(state);
      attributes.push_back(std::move(state_elem));
   }

   target_list.splice(std::move(job_list));

   DRETURN_VOID;
}

/** @brief Start the document: the element of the queue instances and the element of the job sections
 * @param os stream to write to
 * @param parameter the call's parameters
 * @return none
 */
void ocs::QStatDefaultViewXML::report_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   queue_list_elem = ocs::tom::ElementPtr<ocs::tom::XMLE>::create();
   auto attr_elem = ocs::tom::ElementPtr<ocs::tom::XMLA>::create();
   attr_elem->set<XMLA_Name>("queue_info");
   queue_list_elem->set<XMLE_Element>(std::move(attr_elem));
   queue_list_elem->set<XMLE_Print>(true);

   job_list_elem = ocs::tom::ElementPtr<ocs::tom::XMLE>::create();
   auto attr_elem_xmla = ocs::tom::ElementPtr<ocs::tom::XMLA>::create();
   attr_elem_xmla->set<XMLA_Name>("job_info");
   job_list_elem->set<XMLE_Element>(std::move(attr_elem_xmla));
   job_list_elem->set<XMLE_Print>(true);
   job_list_elem->set<XMLE_List>(ocs::tom::ListPtr<ocs::tom::XMLE>::create("job_list"));

   DRETURN_VOID;
}

/** @brief Write the document that was built from the hook calls
 * @param os stream to write to
 * @param parameter the call's parameters
 * @return none
 */
void ocs::QStatDefaultViewXML::report_finished(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   auto XML_out = ocs::tom::ListPtr<ocs::tom::XMLE>::create("job_info");

   if (queue_list_elem) {
      XML_out->push_back(std::move(queue_list_elem));
   }
   if (job_list_elem) {
      XML_out->push_back(std::move(job_list_elem));
   }

   // the head takes the list over
   const auto xml_head = xml_getHead("job_info", std::move(XML_out), nullptr);
   lWriteElemXMLTo(*xml_head, os);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_queue_section_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_queue_section_finished(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/** @brief File the queue instance just filled under the queue instances, or drop it without `-f`
 * @param os stream to write to
 * @param qname the queue instance
 * @param parameter the call's parameters
 * @return none
 */
void ocs::QStatDefaultViewXML::report_queue_finished(std::ostream &os, const char* qname, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   if (parameter.show_ & QSTAT_DISPLAY_FULL) {
      if (queue_elem == nullptr) {
         DPRINTF("Illegal State: ctx->queue_elem is nullptr !!!\n");
         TerminationManager::trigger_abort();
      }
      DPRINTF("add queue_info for queue %s to queue_list\n", qname );

      auto queue_list = queue_list_elem->get_as<XMLE_List, ocs::tom::XMLE>();
      if (queue_list == nullptr) {
         DPRINTF("Had empty queue list, create new one\n");
         auto created = ocs::tom::ListPtr<ocs::tom::XMLE>::create("Queue-List");
         queue_list = *created;
         queue_list_elem->set<XMLE_List>(std::move(created));
      }
      queue_list.push_back(std::move(queue_elem));
   } else {
      queue_elem.reset();
   }

   DRETURN_VOID;
}

/** @brief Start a new queue instance element, with `-f` only
 * @param os stream to write to
 * @param qname the queue instance
 * @param parameter the call's parameters
 * @return none
 */
void ocs::QStatDefaultViewXML::report_queue_started(std::ostream &os, const char* qname, QStatParameter &parameter) {
   DENTER(TOP_LAYER);


   if (parameter.show_ & QSTAT_DISPLAY_FULL) {

      if (queue_elem != nullptr) {
         DPRINTF("Illegal state: ctx->queue_elem has to be nullptr");
         TerminationManager::trigger_abort();
      }

      DPRINTF("Create ctx->queue_elem for queue %s\n", qname);

      auto temp = ocs::tom::ElementPtr<ocs::tom::XMLE>::create();
      temp->set<XMLE_Print>(false);
      queue_elem = ocs::tom::ElementPtr<ocs::tom::XMLE>::create();
      auto attribute_list = ocs::tom::ListPtr<ocs::tom::XMLE>::create("attributes");
      temp->set<XMLE_List>(std::move(attribute_list));
      queue_elem->set<XMLE_Element>(std::move(temp));
   }

   DRETURN_VOID;
}

/** @brief Add the summary columns of the queue instance being filled
 * @param os stream to write to
 * @param qname the queue instance
 * @param summary the values of the columns
 * @param parameter the call's parameters
 * @return none
 */
void ocs::QStatDefaultViewXML::report_queue_summary(std::ostream &os, const char *qname, queue_summary_t *summary,
                                                    QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   if (queue_elem == nullptr) {
      DPRINTF("Ilegal state: ctx->queue_elem must not be nullptr");
      TerminationManager::trigger_abort();
   }
   // the queue instance's element nests an element whose list holds its attributes
   const auto attribute_list =
      queue_elem->get_as<XMLE_Element, ocs::tom::XMLE>().get_as<XMLE_List, ocs::tom::XMLE>();

   xml_append_Attr_S(attribute_list, "name", qname);
   xml_append_Attr_S(attribute_list, "qtype", summary->queue_type.c_str());

   /* number of used/free slots */
   xml_append_Attr_U(attribute_list, "slots_used", summary->used_slots);
   xml_append_Attr_U(attribute_list, "slots_resv", summary->resv_slots);
   xml_append_Attr_U(attribute_list, "slots_total", summary->total_slots);

   /* load avg */
   if (summary->has_load_value && summary->has_load_value_from_object) {
      xml_append_Attr_D(attribute_list, "load_avg", summary->load_avg);
   }

   /* arch */
   if (!summary->arch.empty()) {
      xml_append_Attr_S(attribute_list, "arch", summary->arch.c_str());
   }

   xml_append_Attr_S(attribute_list, "state", summary->state.c_str());

   DRETURN_VOID;
}

/** @brief Add the reason of a load alarm to the queue instance being filled
 * @param os stream to write to
 * @param qname the queue instance
 * @param reason the reason
 * @return none
 */
void ocs::QStatDefaultViewXML::report_queue_load_alarm(std::ostream &os, const char* qname, const char* reason) {
   DENTER(TOP_LAYER);

   // the queue instance's element nests an element whose list holds its attributes
   const auto attribute_list =
      queue_elem->get_as<XMLE_Element, ocs::tom::XMLE>().get_as<XMLE_List, ocs::tom::XMLE>();
   xml_append_Attr_S(attribute_list, "load-alarm-reason", reason);

   DRETURN_VOID;
}

/** @brief Add the reason of a suspend alarm to the queue instance being filled
 * @param os stream to write to
 * @param qname the queue instance
 * @param reason the reason
 * @return none
 */
void ocs::QStatDefaultViewXML::report_queue_suspend_alarm(std::ostream &os, const char* qname, const char* reason) {
   DENTER(TOP_LAYER);

   // the queue instance's element nests an element whose list holds its attributes
   const auto attribute_list =
      queue_elem->get_as<XMLE_Element, ocs::tom::XMLE>().get_as<XMLE_List, ocs::tom::XMLE>();
   xml_append_Attr_S(attribute_list, "suspend-alarm-reason", reason);

   DRETURN_VOID;
}

/** @brief Add a message to the queue instance being filled
 * @param os stream to write to
 * @param qname the queue instance
 * @param message the message
 * @return none
 */
void ocs::QStatDefaultViewXML::report_queue_message(std::ostream &os, const char* qname, const char *message) {
   DENTER(TOP_LAYER);

   // the queue instance's element nests an element whose list holds its attributes
   const auto attribute_list =
      queue_elem->get_as<XMLE_Element, ocs::tom::XMLE>().get_as<XMLE_List, ocs::tom::XMLE>();

   xml_append_Attr_S(attribute_list, "message", message);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_queue_resource_started(std::ostream &os, const char* name) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_queue_resource_finished(std::ostream &os, const char* name) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/** @brief Report one resource of the queue instance
 * @param os stream to write to
 * @param resource the complex entry the value belongs to
 * @param dom where the value comes from, e.g. `hl` for a host load value
 * @param name the resource
 * @param value its value
 * @param details the value's origin, printed when the user asked for it
 * @return none
 */
void ocs::QStatDefaultViewXML::report_queue_resource(std::ostream &os,
                                                     const ocs::tom::ConstElement<ocs::tom::CE> resource,
                                                     const char *dom, const char *name, const char *value,
                                                     const char *details) {
   DENTER(TOP_LAYER);

   // the queue instance's element nests an element whose list holds its attributes
   const auto attribute_list =
      queue_elem->get_as<XMLE_Element, ocs::tom::XMLE>().get_as<XMLE_List, ocs::tom::XMLE>();

   DPRINTF("queue resource: %s, %s, %s, %s\n", dom, name, value, details);
   const auto xml_elem = xml_append_Attr_S(attribute_list, "resource", value);
   xml_addAttribute(xml_elem, "name", name);
   xml_addAttribute(xml_elem, "type", dom);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_queue_jobs_started(std::ostream &os, const char* qname, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   qstat_xml_create_job_list();
   DRETURN_VOID;
}

/** @brief File the running jobs of the section under the queue instance, or under the queue instances without `-f`
 *
 * An empty section is dropped.
 *
 * @param os stream to write to
 * @param qname the queue instance
 * @param parameter the call's parameters
 * @return none
 */
void ocs::QStatDefaultViewXML::report_queue_jobs_finished(std::ostream &os, const char *qname,
                                                          QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   if (job_list->front() != nullptr) {
      // the element whose list takes the jobs
      const auto &owner = (parameter.show_ & QSTAT_DISPLAY_FULL) ? queue_elem : queue_list_elem;
      auto l_job_list = owner->get_as<XMLE_List, ocs::tom::XMLE>();
      if (l_job_list == nullptr) {
         auto created = ocs::tom::ListPtr<ocs::tom::XMLE>::create("job_list");
         l_job_list = *created;
         owner->set<XMLE_List>(std::move(created));
      }
      qstat_xml_finish_job_list("running", l_job_list);
   } else {
      job_list.reset();
   }

   DRETURN_VOID;
}


void ocs::QStatDefaultViewXML::report_pending_jobs_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   qstat_xml_create_job_list();
   DRETURN_VOID;
}

/** @brief File the jobs of the section under the job sections, as `pending`
 * @param os stream to write to
 * @return none
 */
void ocs::QStatDefaultViewXML::report_pending_jobs_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   const auto target_list = job_list_elem->get_as<XMLE_List, ocs::tom::XMLE>();
   qstat_xml_finish_job_list("pending", target_list);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_finished_jobs_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   qstat_xml_create_job_list();

   DRETURN_VOID;
}

/** @brief File the jobs of the section under the job sections, as `finished_retained`
 * @param os stream to write to
 * @return none
 */
void ocs::QStatDefaultViewXML::report_finished_jobs_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   const auto target_list = job_list_elem->get_as<XMLE_List, ocs::tom::XMLE>();
   /* CS-1908 U7: renamed from "finished" — under retention this tag holds the
    * ja_tasks currently retained in the finished-history window (not the
    * transient JOBS-WAITING-FOR-ACCOUNTING queue it modeled before). */
   qstat_xml_finish_job_list("finished_retained", target_list);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_error_jobs_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   qstat_xml_create_job_list();

   DRETURN_VOID;
}

/** @brief File the jobs of the section under the job sections, as `error`
 * @param os stream to write to
 * @return none
 */
void ocs::QStatDefaultViewXML::report_error_jobs_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   const auto target_list = job_list_elem->get_as<XMLE_List, ocs::tom::XMLE>();
   qstat_xml_finish_job_list("error", target_list);

   DRETURN_VOID;
}

/*
** start and finished functions needed for clients/common/sge_qstat.c do work
*/
void ocs::QStatDefaultViewXML::report_sub_tasks_started(std::ostream &os) {
}

void ocs::QStatDefaultViewXML::report_hard_resources_started(std::ostream &os, int some_value) {
}

void ocs::QStatDefaultViewXML::report_sub_tasks_finished(std::ostream &os) {
}


/** @brief Start a new job element and fill in the summary columns
 * @param os stream to write to
 * @param jid the job id
 * @param summary the values of the columns
 * @param parameter the call's parameters
 * @param model the fetched lists
 * @return none
 */
void ocs::QStatDefaultViewXML::report_job(std::ostream &os, const uint32_t jid, job_summary_t *summary,
                                          QStatParameter &parameter, QStatModelBase &model) {
   DENTER(TOP_LAYER);
   int sge_ext, tsk_ext, sge_urg, sge_pri, sge_time;
   dstring ds = DSTRING_INIT;
   // @todo: static init is frozen at first call — re-evaluate if this code is ever reused in a long-running process
   static bool compat = sge_getenv("SGE_QSTAT_SGE_COMPATIBILITY") != nullptr;

   job_elem = ocs::tom::ElementPtr<ocs::tom::XMLE>::create();
   auto attributes = ocs::tom::ListPtr<ocs::tom::XMLE>::create("attributes");
   const auto attribute_list = *attributes;
   job_elem->set<XMLE_List>(std::move(attributes));

   sge_ext = (parameter.show_ & QSTAT_DISPLAY_EXTENDED) ? true : false;
   tsk_ext = (parameter.show_ & QSTAT_DISPLAY_TASKS) ? true : false;
   sge_urg = (parameter.show_ & QSTAT_DISPLAY_URGENCY) ? true : false;
   sge_pri = (parameter.show_ & QSTAT_DISPLAY_PRIORITY) ? true : false;
   sge_time = !sge_ext;
   sge_time = sge_time | tsk_ext | sge_urg | sge_pri;

   xml_append_Attr_U(attribute_list, "JB_job_number", jid);
   xml_append_Attr_D(attribute_list, "JAT_prio", summary->nprior);
   if( sge_ext) {
      xml_append_Attr_D(attribute_list, "JAT_ntix", summary->ntckts);
   }

   if (sge_urg) {
      xml_append_Attr_D(attribute_list, "JB_nurg", summary->nurg);
      xml_append_Attr_D8(attribute_list, "JB_urg", summary->urg);
      xml_append_Attr_D8(attribute_list, "JB_rrcontr", summary->rrcontr);
      xml_append_Attr_D8(attribute_list, "JB_wtcontr", summary->wtcontr);
      xml_append_Attr_D8(attribute_list, "JB_dlcontr", summary->dlcontr);
   }

   if (sge_pri) {
      xml_append_Attr_D(attribute_list, "JB_nppri", summary->nppri);
      xml_append_Attr_I(attribute_list, "JB_priority", summary->priority);
   }

   xml_append_Attr_S(attribute_list, "JB_name", summary->name);
   xml_append_Attr_S(attribute_list, "JB_owner", summary->user);

   if (sge_ext) {
      xml_append_Attr_S(attribute_list, "JB_project", summary->project);
      xml_append_Attr_S(attribute_list, "JB_department", summary->department);
   }


   xml_append_Attr_S(attribute_list, "state", summary->state);

   if (sge_time) {
      uint64_t timestamp;
      const char *attrib;
      if (summary->is_running) {
         attrib = "JAT_start_time";
         timestamp = summary->start_time;
      } else {
         attrib = "JAT_submission_time";
         timestamp = summary->submit_time;
      }
      xml_append_Attr_S(attribute_list, attrib, sge_ctime64(timestamp, &ds, true, compat ? false : true));
   }

   /* deadline time */
   if (sge_urg && summary->deadline > 0) {
      xml_append_Attr_S(attribute_list, "JB_deadline",
                        sge_ctime64(summary->deadline, &ds, true, compat ? false : true));
   }

   if (sge_ext) {
      /* scaled cpu usage */
      if (summary->has_cpu_usage) {
         xml_append_Attr_D(attribute_list, "cpu_usage", summary->cpu_usage);
      }
      /* scaled mem usage */
      if (summary->has_mem_usage)
         xml_append_Attr_D(attribute_list, "mem_usage", summary->mem_usage);

      /* scaled io usage */
      if (summary->has_io_usage)
         xml_append_Attr_D(attribute_list, "io_usage", summary->io_usage);

      if (sge_ext ||summary->is_queue_assigned) {
         xml_append_Attr_U(attribute_list, "tickets", summary->tickets);
         xml_append_Attr_U(attribute_list, "JB_override_tickets", summary->override_tickets);
         xml_append_Attr_U(attribute_list, "JB_jobshare", summary->share);
         xml_append_Attr_U(attribute_list, "otickets", summary->otickets);
         xml_append_Attr_U(attribute_list, "ftickets", summary->ftickets);
         xml_append_Attr_U(attribute_list, "stickets", summary->stickets);
         xml_append_Attr_D(attribute_list, "JAT_share", summary->share);
      }
   }

   /* if not full listing we need the queue's name in each line */
   if (!(parameter.show_ & QSTAT_DISPLAY_FULL)) {
      xml_append_Attr_S(attribute_list, "queue_name", summary->queue);
   }

   if ((parameter.group_opt_ & GROUP_NO_PETASK_GROUPS)) {
      /* MASTER/SLAVE information needed only to show parallel job distribution */
      xml_append_Attr_S(attribute_list, "master", summary->master);
   }

   xml_append_Attr_U(attribute_list, "slots", summary->slots);

   if (summary->task_id && summary->is_array) {
      xml_append_Attr_S(attribute_list, "tasks", summary->task_id);
   }

   sge_dstring_free(&ds);
   DRETURN_VOID;
}

/** @brief File the job just filled under the current section
 * @param os stream to write to
 * @param jid the job id
 * @return none
 */
void ocs::QStatDefaultViewXML::report_job_finished(std::ostream &os, uint32_t jid) {
   DENTER(TOP_LAYER);

   // without a section the job is dropped, as appending to no list did
   if (job_list && job_elem) {
      job_list->push_back(std::move(job_elem));
   } else {
      job_elem.reset();
   }

   DRETURN_VOID;
}

/** @brief Add the columns of a sub task to the job being filled
 * @param os stream to write to
 * @param summary the values of the task's columns
 * @return none
 */
void ocs::QStatDefaultViewXML::report_sub_task(std::ostream &os, task_summary_t *summary) {
   DENTER(TOP_LAYER);

   const ocs::tom::List<ocs::tom::XMLE> attribute_list = job_elem->get_as<XMLE_List, ocs::tom::XMLE>();

   if (summary->task_id) {
      xml_append_Attr_S(attribute_list, "task-id", summary->task_id);
   }
   if (summary->has_cpu_usage) {
      xml_append_Attr_D(attribute_list, "cpu-usage", summary->cpu_usage);
   }
   if (summary->has_mem_usage) {
      xml_append_Attr_D(attribute_list, "mem-usage", summary->mem_usage);
   }
   if (summary->has_io_usage) {
      xml_append_Attr_D(attribute_list, "io-usage", summary->io_usage);
   }
   if (summary->has_exit_status) {
      xml_append_Attr_U(attribute_list, "stat", summary->exit_status);
   }

   DRETURN_VOID;
}

/** @brief Add an additional job information to the job being filled; those shown elsewhere are skipped
 * @param os stream to write to
 * @param name which information it is
 * @param value its value
 * @return none
 */
void ocs::QStatDefaultViewXML::report_additional_info(std::ostream &os, const job_additional_info_t name,
                                                      const char *value) {
   DENTER(TOP_LAYER);

   switch(name) {
      case CHECKPOINT_ENV:
      case MASTER_QUEUE:
      case REQUESTED_PE:
      case GRANTED_PE:
         DPRINTF("Skip additional info %s(%d) = %s\n", ADDITIONAL_TAG_NAMES[name], name, value);
         DRETURN_VOID;
      default:
         {
            /*lListElem *xml_elem = nullptr;*/
            const ocs::tom::List<ocs::tom::XMLE> attribute_list = job_elem->get_as<XMLE_List, ocs::tom::XMLE>();

            DPRINTF("Add additional info %s(%d) = %s\n", ADDITIONAL_TAG_NAMES[name], name, value);
            xml_append_Attr_S(attribute_list, ADDITIONAL_TAG_NAMES[name], value);
         }
   }
   DRETURN_VOID;
}

/** @brief Add the requested parallel environment to the job being filled
 * @param os stream to write to
 * @param pe_name the parallel environment
 * @param pe_range the requested slot range
 * @return none
 */
void ocs::QStatDefaultViewXML::report_requested_pe(std::ostream &os, const char* pe_name, const char* pe_range) {
   DENTER(TOP_LAYER);

   const ocs::tom::List<ocs::tom::XMLE> attribute_list = job_elem->get_as<XMLE_List, ocs::tom::XMLE>();

   const auto xml_elem = xml_append_Attr_S(attribute_list, "requested_pe", pe_range);
   xml_addAttribute(xml_elem, "name", pe_name);

   DRETURN_VOID;
}

/** @brief Add the granted parallel environment to the job being filled
 * @param os stream to write to
 * @param pe_name the parallel environment
 * @param pe_slots the granted slots
 * @return none
 */
void ocs::QStatDefaultViewXML::report_granted_pe(std::ostream &os, const char* pe_name, int pe_slots) {
   DENTER(TOP_LAYER);

   const ocs::tom::List<ocs::tom::XMLE> attribute_list = job_elem->get_as<XMLE_List, ocs::tom::XMLE>();

   const auto xml_elem = xml_append_Attr_I(attribute_list, "granted_pe", pe_slots);
   xml_addAttribute(xml_elem, "name", pe_name);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_default_request_started(std::ostream &os) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_default_request_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/** @brief Add a default resource request to the job being filled
 * @param os stream to write to
 * @param name the resource
 * @param value the requested value
 * @return none
 */
void ocs::QStatDefaultViewXML::report_default_request(std::ostream &os, const char* name, const char* value) {
   DENTER(TOP_LAYER);

   const ocs::tom::List<ocs::tom::XMLE> attribute_list = job_elem->get_as<XMLE_List, ocs::tom::XMLE>();

   const auto xml_elem = xml_append_Attr_S(attribute_list, "def_hard_request", value);
   xml_addAttribute(xml_elem, "name", name);

   DRETURN_VOID;
}

/** @brief Report one hard resource request
 * @param os stream to write to
 * @param scope which request scope the request belongs to: global, master or slave
 * @param resource the complex entry the value belongs to
 * @param name the resource
 * @param value the requested value
 * @param uc the urgency contribution of the request
 * @return none
 */
void ocs::QStatDefaultViewXML::report_hard_resource(std::ostream &os, const int scope,
                                                    const ocs::tom::ConstElement<ocs::tom::CE> resource,
                                                    const char *name, const char *value, const double uc) {
   DENTER(TOP_LAYER);

   const char *attrib_name;
   switch(scope) {
      case JRS_SCOPE_MASTER:
         attrib_name = "master_hard_request";
         break;
      case JRS_SCOPE_SLAVE:
         attrib_name = "slave_hard_request";
         break;
      case JRS_SCOPE_GLOBAL:
      default:
         attrib_name = "hard_request";
         break;
   }

   const ocs::tom::List<ocs::tom::XMLE> attribute_list = job_elem->get_as<XMLE_List, ocs::tom::XMLE>();

   const auto xml_elem = xml_append_Attr_S(attribute_list, attrib_name, value);
   xml_addAttribute(xml_elem, "name", name);
   xml_addAttributeD(xml_elem, "resource_contribution", uc);

   DRETURN_VOID;
}

/** @brief Report one soft resource request
 * @param os stream to write to
 * @param scope which request scope the request belongs to: global, master or slave
 * @param resource the complex entry the value belongs to
 * @param name the resource
 * @param value the requested value
 * @param uc the urgency contribution of the request
 * @return none
 */
void ocs::QStatDefaultViewXML::report_soft_resource(std::ostream &os, const int scope,
                                                    const ocs::tom::ConstElement<ocs::tom::CE> resource,
                                                    const char *name, const char *value, const double uc) {
   DENTER(TOP_LAYER);

   const char *attrib_name;
   switch(scope) {
      case JRS_SCOPE_MASTER:
         attrib_name = "master_soft_request";
         break;
      case JRS_SCOPE_SLAVE:
         attrib_name = "slave_soft_request";
         break;
      case JRS_SCOPE_GLOBAL:
      default:
         attrib_name = "soft_request";
         break;
   }

   const ocs::tom::List<ocs::tom::XMLE> attribute_list = job_elem->get_as<XMLE_List, ocs::tom::XMLE>();

   const auto xml_elem = xml_append_Attr_S(attribute_list, attrib_name, value);
   xml_addAttribute(xml_elem, "name", name);

   DRETURN_VOID;
}

/** @brief Add a hard queue request to the job being filled
 * @param os stream to write to
 * @param scope the scope of the request, a JRS_SCOPE_* value
 * @param name the requested queue
 * @return none
 */
void ocs::QStatDefaultViewXML::report_hard_requested_queue(std::ostream &os, int scope, const char* name) {
   DENTER(TOP_LAYER);

   const ocs::tom::List<ocs::tom::XMLE> attribute_list = job_elem->get_as<XMLE_List, ocs::tom::XMLE>();

   const char *attrib_name;
   switch(scope) {
      case JRS_SCOPE_MASTER:
         attrib_name = "master_hard_req_queue";
         break;
      case JRS_SCOPE_SLAVE:
         attrib_name = "slave_hard_req_queue";
         break;
      case JRS_SCOPE_GLOBAL:
      default:
         attrib_name = "hard_req_queue";
         break;
   }
   xml_append_Attr_S(attribute_list, attrib_name, name);

   DRETURN_VOID;
}

/** @brief Add a soft queue request to the job being filled
 * @param os stream to write to
 * @param scope the scope of the request, a JRS_SCOPE_* value
 * @param name the requested queue
 * @return none
 */
void ocs::QStatDefaultViewXML::report_soft_requested_queue(std::ostream &os, int scope, const char* name) {
   DENTER(TOP_LAYER);

   const ocs::tom::List<ocs::tom::XMLE> attribute_list = job_elem->get_as<XMLE_List, ocs::tom::XMLE>();

   const char *attrib_name;
   switch(scope) {
      case JRS_SCOPE_MASTER:
         attrib_name = "master_soft_req_queue";
         break;
      case JRS_SCOPE_SLAVE:
         attrib_name = "slave_soft_req_queue";
         break;
      case JRS_SCOPE_GLOBAL:
      default:
         attrib_name = "soft_req_queue";
         break;
   }
   xml_append_Attr_S(attribute_list, attrib_name, name);

   DRETURN_VOID;
}

/** @brief Add a requested predecessor job to the job being filled
 * @param os stream to write to
 * @param name the job as requested
 * @return none
 */
void ocs::QStatDefaultViewXML::report_predecessor_requested(std::ostream &os, const char* name) {
   DENTER(TOP_LAYER);

   const ocs::tom::List<ocs::tom::XMLE> attribute_list = job_elem->get_as<XMLE_List, ocs::tom::XMLE>();

   xml_append_Attr_S(attribute_list, "predecessor_jobs_req", name);

   DRETURN_VOID;
}

/** @brief Add a predecessor job to the job being filled
 * @param os stream to write to
 * @param jid the predecessor's job id
 * @return none
 */
void ocs::QStatDefaultViewXML::report_predecessor(std::ostream &os, uint32_t jid) {
   DENTER(TOP_LAYER);

   const ocs::tom::List<ocs::tom::XMLE> attribute_list = job_elem->get_as<XMLE_List, ocs::tom::XMLE>();

   xml_append_Attr_U(attribute_list, "predecessor_jobs", jid);

   DRETURN_VOID;
}

/** @brief Add a requested array predecessor job to the job being filled
 * @param os stream to write to
 * @param name the job as requested
 * @return none
 */
void ocs::QStatDefaultViewXML::report_ad_predecessor_requested(std::ostream &os, const char* name) {
   DENTER(TOP_LAYER);

   const ocs::tom::List<ocs::tom::XMLE> attribute_list = job_elem->get_as<XMLE_List, ocs::tom::XMLE>();

   xml_append_Attr_S(attribute_list, "ad_predecessor_jobs_req", name);

   DRETURN_VOID;
}

/** @brief Add an array predecessor job to the job being filled
 * @param os stream to write to
 * @param jid the predecessor's job id
 * @return none
 */
void ocs::QStatDefaultViewXML::report_ad_predecessor(std::ostream &os, uint32_t jid) {
   DENTER(TOP_LAYER);

   const ocs::tom::List<ocs::tom::XMLE> attribute_list = job_elem->get_as<XMLE_List, ocs::tom::XMLE>();

   xml_append_Attr_U(attribute_list, "ad_predecessor_jobs", jid);

   DRETURN_VOID;
}


void ocs::QStatDefaultViewXML::report_hard_resources_finished(std::ostream &os) {
}

void ocs::QStatDefaultViewXML::report_soft_resources_started(std::ostream &os, int scope) {
}

void ocs::QStatDefaultViewXML::report_soft_resources_finished(std::ostream &os) {
}

void ocs::QStatDefaultViewXML::report_soft_requested_queues_started(std::ostream &os, int scope) {
}

void ocs::QStatDefaultViewXML::report_soft_requested_queues_finished(std::ostream &os) {
}

void ocs::QStatDefaultViewXML::report_hard_requested_queues_started(std::ostream &os, int scope) {
}

void ocs::QStatDefaultViewXML::report_hard_requested_queues_finished(std::ostream &os) {
}

void ocs::QStatDefaultViewXML::report_predecessors_requested_started(std::ostream &os) {
}

void ocs::QStatDefaultViewXML::report_predecessors_requested_finished(std::ostream &os) {
}

void ocs::QStatDefaultViewXML::report_predecessors_started(std::ostream &os) {
}

void ocs::QStatDefaultViewXML::report_predecessors_finished(std::ostream &os) {
}

void ocs::QStatDefaultViewXML::report_ad_predecessors_requested_started(std::ostream &os) {
}

void ocs::QStatDefaultViewXML::report_ad_predecessors_requested_finished(std::ostream &os) {
}

void ocs::QStatDefaultViewXML::report_ad_predecessors_started(std::ostream &os) {
}

void ocs::QStatDefaultViewXML::report_ad_predecessors_finished(std::ostream &os) {
}

void ocs::QStatDefaultViewXML::report_binding_started(std::ostream &os) {
}

void ocs::QStatDefaultViewXML::report_binding_finished(std::ostream &os) {
}

/** @brief Add the binding to the job being filled
 * @param os stream to write to
 * @param binding the binding
 * @return none
 */
void ocs::QStatDefaultViewXML::report_binding(std::ostream &os, const char *binding) {
   DENTER(TOP_LAYER);
   const ocs::tom::List<ocs::tom::XMLE> attribute_list = job_elem->get_as<XMLE_List, ocs::tom::XMLE>();

   xml_append_Attr_S(attribute_list, "binding", binding);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_binding_attribute(std::ostream &os, const char *name, const char *value) {
}

void ocs::QStatDefaultViewXML::report_binding_attribute(std::ostream &os, const char *name, uint32_t value) {
}
