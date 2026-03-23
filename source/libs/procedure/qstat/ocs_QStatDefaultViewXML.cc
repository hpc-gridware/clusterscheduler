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

#include <cstdio>
#include <cstdlib>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_log.h"
#include "uti/sge_time.h"

#include "sgeobj/cull/sge_all_listsL.h"

#include "sgeobj/parse.h"
#include "sgeobj/sge_cull_xml.h"
#include "sgeobj/sge_job.h"

#include "ocs_QStatDefaultViewXML.h"

static const char* ADDITIONAL_TAG_NAMES[] = {
   "ERROR",
   "checkpoint_env",
   "master_queue",
   "full_job_name"
};

void ocs::QStatDefaultViewXML::qstat_xml_create_job_list() {
   DENTER(TOP_LAYER);

   SGE_ASSERT(job_list == nullptr);
   job_list = lCreateList("job_list", XMLE_Type);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::qstat_xml_finish_job_list(const char* state, lList* target_list) {
   DENTER(TOP_LAYER);
   lListElem *l_job_elem = nullptr;
   lListElem *state_elem = nullptr;
   lList *attributes = nullptr;
   lList *l_job_list = nullptr;

   for_each_rw(l_job_elem, job_list) {
      attributes = lGetListRW(l_job_elem, XMLE_Attribute);
      if (!attributes){
         attributes = lCreateList("attributes", XMLA_Type);
         lSetList(l_job_elem, XMLE_Attribute, attributes);
      }
      state_elem = lCreateElem(XMLA_Type);
      lSetString(state_elem, XMLA_Name, "state");
      lSetString(state_elem, XMLA_Value, state);
      lAppendElem(attributes, state_elem);

      lAppendElem(l_job_list, l_job_elem);
   }

   lAddList(target_list, &job_list);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_started(std::ostream &os) {
   DENTER(TOP_LAYER);

   queue_list_elem = lCreateElem(XMLE_Type);
   lListElem *attr_elem = lCreateElem(XMLA_Type);
   lSetString(attr_elem, XMLA_Name, "queue_info");
   lSetObject(queue_list_elem, XMLE_Element, attr_elem);
   lSetBool(queue_list_elem, XMLE_Print, true);

   job_list_elem = lCreateElem(XMLE_Type);
   attr_elem = lCreateElem(XMLA_Type);
   lSetString(attr_elem, XMLA_Name, "job_info");
   lSetObject(job_list_elem, XMLE_Element, attr_elem);
   lSetBool(job_list_elem, XMLE_Print, true);
   lSetList(job_list_elem, XMLE_List, lCreateList("job_list", XMLE_Type));

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_finished(std::ostream &os) {
   DENTER(TOP_LAYER);

   lList *XML_out = lCreateList("job_info", XMLE_Type);

   lAppendElem(XML_out, queue_list_elem);
   queue_list_elem = nullptr;

   lAppendElem(XML_out, job_list_elem);
   job_list_elem = nullptr;

   lListElem *xml_elem = xml_getHead("job_info", XML_out, nullptr);
   lWriteElemXMLTo(xml_elem, os);
   lFreeElem(&xml_elem);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_queue_finished(std::ostream &os, const char* qname, QStatParameter &parameter) {

   DENTER(TOP_LAYER);

   if (parameter.full_listing_ & QSTAT_DISPLAY_FULL) {
      if (queue_elem == nullptr) {
         DPRINTF("Illegal State: ctx->queue_elem is nullptr !!!\n");
         TerminationManager::trigger_abort();
      }
      DPRINTF("add queue_info for queue %s to queue_list\n", qname );

      lList* queue_list = lGetListRW(queue_list_elem, XMLE_List);
      if (queue_list == nullptr) {
         DPRINTF("Had empty queue list, create new one\n");
         queue_list = lCreateList("Queue-List", XMLE_Type);
         lSetList(queue_list_elem, XMLE_List, queue_list);
      }
      lAppendElem(queue_list, queue_elem);
      queue_elem = nullptr;
   } else {
      lFreeElem(&queue_elem);
   }

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_queue_started(std::ostream &os, const char* qname, QStatParameter &parameter) {
   lList *attribute_list = nullptr;
   lListElem *temp = nullptr;

   DENTER(TOP_LAYER);

   if (parameter.full_listing_ & QSTAT_DISPLAY_FULL) {

      if (queue_elem != nullptr) {
         DPRINTF("Illegal state: ctx->queue_elem has to be nullptr");
         TerminationManager::trigger_abort();
      }

      DPRINTF("Create ctx->queue_elem for queue %s\n", qname);

      temp = lCreateElem(XMLE_Type);
      lSetBool(temp, XMLE_Print, false);
      queue_elem = lCreateElem(XMLE_Type);
      attribute_list = lCreateList("attributes", XMLE_Type);
      lSetList(temp, XMLE_List, attribute_list);
      lSetObject(queue_elem, XMLE_Element, temp);
   }

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_queue_summary(std::ostream &os, const char* qname, queue_summary_t *summary, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   lList *attribute_list = nullptr;
   lListElem *xml_elem = nullptr;

   if (queue_elem == nullptr) {
      DPRINTF("Ilegal state: ctx->queue_elem must not be nullptr");
      TerminationManager::trigger_abort();
   }
   xml_elem = lGetObject(queue_elem, XMLE_Element);
   attribute_list = lGetListRW(xml_elem, XMLE_List);

   xml_append_Attr_S(attribute_list, "name", qname);
   xml_append_Attr_S(attribute_list, "qtype", summary->queue_type);

   /* number of used/free slots */
   xml_append_Attr_U(attribute_list, "slots_used", summary->used_slots);
   xml_append_Attr_U(attribute_list, "slots_resv", summary->resv_slots);
   xml_append_Attr_U(attribute_list, "slots_total", summary->total_slots);

   /* load avg */
   if (summary->has_load_value && summary->has_load_value_from_object) {
      xml_append_Attr_D(attribute_list, "load_avg", summary->load_avg);
   }

   /* arch */
   if(summary->arch) {
      xml_append_Attr_S(attribute_list, "arch", summary->arch);
   }

   xml_append_Attr_S(attribute_list, "state", summary->state);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_queue_load_alarm(std::ostream &os, const char* qname, const char* reason) {
   DENTER(TOP_LAYER);

   lListElem *xml_elem = lGetObject(queue_elem, XMLE_Element);
   lList *attribute_list = lGetListRW(xml_elem, XMLE_List);
   xml_append_Attr_S(attribute_list, "load-alarm-reason", reason);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_queue_suspend_alarm(std::ostream &os, const char* qname, const char* reason) {
   DENTER(TOP_LAYER);

   lListElem *xml_elem = lGetObject(queue_elem, XMLE_Element);
   lList *attribute_list = lGetListRW(xml_elem, XMLE_List);
   xml_append_Attr_S(attribute_list, "suspend-alarm-reason", reason);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_queue_message(std::ostream &os, const char* qname, const char *message) {
   DENTER(TOP_LAYER);

   lListElem *xml_elem = lGetObject(queue_elem, XMLE_Element);
   lList *attribute_list = lGetListRW(xml_elem, XMLE_List);

   xml_append_Attr_S(attribute_list, "message", message);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_queue_resource(std::ostream &os, const char* dom, const char* name, const char* value, const char *details) {

   DENTER(TOP_LAYER);

   lListElem *xml_elem = lGetObject(queue_elem, XMLE_Element);
   lList *attribute_list = lGetListRW(xml_elem, XMLE_List);

   DPRINTF("queue resource: %s, %s, %s, %s\n", dom, name, value, details);
   xml_elem = xml_append_Attr_S(attribute_list, "resource", value);
   xml_addAttribute(xml_elem, "name", name);
   xml_addAttribute(xml_elem, "type", dom);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_queue_jobs_started(std::ostream &os, const char* qname) {
   DENTER(TOP_LAYER);
   qstat_xml_create_job_list();
   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_queue_jobs_finished(std::ostream &os, const char* qname, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   if (lFirst(job_list)) {
      lList *l_job_list = nullptr;
      if (parameter.full_listing_ & QSTAT_DISPLAY_FULL ) {
         l_job_list = lGetListRW(queue_elem, XMLE_List);
         if(l_job_list == nullptr) {
            l_job_list = lCreateList("job_list", XMLE_Type);
            lSetList(queue_elem, XMLE_List, l_job_list);
         }
      } else {
        l_job_list = lGetListRW(queue_list_elem, XMLE_List);
         if(l_job_list == nullptr) {
            l_job_list = lCreateList("job_list", XMLE_Type);
            lSetList(queue_list_elem, XMLE_List, l_job_list);
         }
      }
      qstat_xml_finish_job_list("running", l_job_list);
   } else {
      lFreeList(&job_list);
   }

   DRETURN_VOID;
}


void ocs::QStatDefaultViewXML::report_pending_jobs_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   qstat_xml_create_job_list();
   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_pending_jobs_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   lList *target_list = lGetListRW(job_list_elem, XMLE_List);
   qstat_xml_finish_job_list("pending", target_list);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_finished_jobs_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   qstat_xml_create_job_list();

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_finished_jobs_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   lList *target_list = lGetListRW(job_list_elem, XMLE_List);
   qstat_xml_finish_job_list("finished", target_list);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_error_jobs_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   qstat_xml_create_job_list();

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_error_jobs_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   lList *target_list = lGetListRW(job_list_elem, XMLE_List);
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


void ocs::QStatDefaultViewXML::report_job(std::ostream &os, u_long32 jid, job_summary_t *summary, QStatParameter &parameter, QStatGenericModel &model) {
   DENTER(TOP_LAYER);
   int sge_ext, tsk_ext, sge_urg, sge_pri, sge_time;
   dstring ds = DSTRING_INIT;
   lList *attribute_list = nullptr;
   static bool compat = sge_getenv("SGE_QSTAT_SGE_COMPATIBILITY") != nullptr;

   job_elem = lCreateElem(XMLE_Type);
   attribute_list = lCreateList("attributes", XMLE_Type);
   lSetList(job_elem, XMLE_List, attribute_list);

   sge_ext = (parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED) ? true : false;
   tsk_ext = (parameter.full_listing_ & QSTAT_DISPLAY_TASKS) ? true : false;
   sge_urg = (parameter.full_listing_ & QSTAT_DISPLAY_URGENCY) ? true : false;
   sge_pri = (parameter.full_listing_ & QSTAT_DISPLAY_PRIORITY) ? true : false;
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
      u_long64 timestamp;
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
   if (sge_urg) {
      if (summary->deadline) {
         xml_append_Attr_S(attribute_list, "JB_deadline", sge_ctime64(summary->deadline, &ds, true, compat ? false : true));
      }
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
   if (!(parameter.full_listing_ & QSTAT_DISPLAY_FULL)) {
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

void ocs::QStatDefaultViewXML::report_job_finished(std::ostream &os, u_long32 jid) {
   DENTER(TOP_LAYER);

   lAppendElem(job_list, job_elem);
   job_elem = nullptr;

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_sub_task(std::ostream &os, task_summary_t *summary) {
   lListElem *xml_elem = nullptr;
   lList *attribute_list = nullptr;

   DENTER(TOP_LAYER);

   xml_elem = lCreateElem(XMLE_Type);
   attribute_list = lCreateList("attributes", XMLE_Type);
   lSetList(xml_elem, XMLE_List, attribute_list);

   if(summary->task_id) {
      xml_append_Attr_S(attribute_list, "task-id", summary->task_id);
   }

   xml_append_Attr_S(attribute_list, "state", summary->state);

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

   /* add the sub-task to the current job element */
   attribute_list = lGetListRW(job_elem, XMLE_Attribute);
   lAppendElem(attribute_list, xml_elem);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_additional_info(std::ostream &os, job_additional_info_t name, const char* value) {

   DENTER(TOP_LAYER);

   switch(name) {
      case CHECKPOINT_ENV:
      case MASTER_QUEUE:
         DPRINTF("Skip additional info %s(%d) = %s\n", ADDITIONAL_TAG_NAMES[name], name, value);
         DRETURN_VOID;
      default:
         {
            /*lListElem *xml_elem = nullptr;*/
            lList *attribute_list = lGetListRW(job_elem, XMLE_List);

            DPRINTF("Add additional info %s(%d) = %s\n", ADDITIONAL_TAG_NAMES[name], name, value);
            xml_append_Attr_S(attribute_list, ADDITIONAL_TAG_NAMES[name], value);
            /* xml_elem = xml_append_Attr_S(attribute_list, ADDITIONAL_TAG_NAMES[name], value);
            TODO: xml_addAttribute(xmlElem, "name", lGetString(job, JB_pe)); */
         }
   }
   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_requested_pe(std::ostream &os, const char* pe_name, const char* pe_range) {
   lListElem *xml_elem = nullptr;
   lList *attribute_list = lGetListRW(job_elem, XMLE_List);

   DENTER(TOP_LAYER);

   xml_elem = xml_append_Attr_S(attribute_list, "requested_pe", pe_range);
   xml_addAttribute(xml_elem, "name", pe_name);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_granted_pe(std::ostream &os, const char* pe_name, int pe_slots) {
   lListElem *xml_elem = nullptr;
   lList *attribute_list = lGetListRW(job_elem, XMLE_List);

   DENTER(TOP_LAYER);

   xml_elem = xml_append_Attr_I(attribute_list, "granted_pe", pe_slots);
   xml_addAttribute(xml_elem, "name", pe_name);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_request(std::ostream &os, const char* name, const char* value) {
   lListElem *xml_elem = nullptr;
   lList *attribute_list = lGetListRW(job_elem, XMLE_List);

   DENTER(TOP_LAYER);

   xml_elem = xml_append_Attr_S(attribute_list, "def_hard_request", value);
   xml_addAttribute(xml_elem, "name", name);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_hard_resource(std::ostream &os, int scope, const char* name, const char* value, double uc) {
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

   lListElem *xml_elem = nullptr;
   lList *attribute_list = lGetListRW(job_elem, XMLE_List);

   xml_elem = xml_append_Attr_S(attribute_list, attrib_name, value);
   xml_addAttribute(xml_elem, "name", name);
   xml_addAttributeD(xml_elem, "resource_contribution", uc);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_soft_resource(std::ostream &os, int scope, const char* name, const char* value, double uc) {
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

   lListElem *xml_elem = nullptr;
   lList *attribute_list = lGetListRW(job_elem, XMLE_List);

   xml_elem = xml_append_Attr_S(attribute_list, attrib_name, value);
   xml_addAttribute(xml_elem, "name", name);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_hard_requested_queue(std::ostream &os, int scope, const char* name) {
   DENTER(TOP_LAYER);

   lList *attribute_list = lGetListRW(job_elem, XMLE_List);

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

void ocs::QStatDefaultViewXML::report_soft_requested_queue(std::ostream &os, int scope, const char* name) {
   DENTER(TOP_LAYER);

   lList *attribute_list = lGetListRW(job_elem, XMLE_List);

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

void ocs::QStatDefaultViewXML::report_predecessor_requested(std::ostream &os, const char* name) {
   lList *attribute_list = lGetListRW(job_elem, XMLE_List);

   DENTER(TOP_LAYER);

   xml_append_Attr_S(attribute_list, "predecessor_jobs_req", name);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_predecessor(std::ostream &os, u_long32 jid) {
   lList *attribute_list = lGetListRW(job_elem, XMLE_List);

   DENTER(TOP_LAYER);

   xml_append_Attr_U(attribute_list, "predecessor_jobs", jid);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_ad_predecessor_requested(std::ostream &os, const char* name) {
   lList *attribute_list = lGetListRW(job_elem, XMLE_List);

   DENTER(TOP_LAYER);

   xml_append_Attr_S(attribute_list, "ad_predecessor_jobs_req", name);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_ad_predecessor(std::ostream &os, u_long32 jid) {
   lList *attribute_list = lGetListRW(job_elem, XMLE_List);

   DENTER(TOP_LAYER);

   xml_append_Attr_U(attribute_list, "ad_predecessor_jobs", jid);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewXML::report_binding(std::ostream &os, const char *binding) {
   DENTER(TOP_LAYER);
   lList *attribute_list = lGetListRW(job_elem, XMLE_List);

   xml_append_Attr_S(attribute_list, "binding", binding);
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