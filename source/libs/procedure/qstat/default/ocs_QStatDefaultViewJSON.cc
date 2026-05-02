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

#include <sstream>
#include <format>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_host.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/parse.h"

#include "qstat/default/ocs_QStatDefaultViewJSON.h"

void ocs::QStatDefaultViewJSON::report_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   os << std::string(indent * 3, ' ') << "{\n";

   indent++;
   os << std::string(indent * 3, ' ') << "\"$schema\": \"https://json-schema.org/draft/2020-12/schema\",\n";
   os << std::string(indent * 3, ' ') <<
         R"("$id": "https://raw.githubusercontent.com/hpc-gridware/clusterscheduler/master/source/dist/util/resources/json-schemas/v9.2/ocs-qstat-)";
   if (parameter.show_ & QSTAT_DISPLAY_FULL) {
      os << "full";
   } else {
      os << "reduced";
   }
   os << ".schema.json\"";

   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_finished(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   if (within_running_section) {
      os << "\n";
      indent--;
      os << std::string(indent * 3, ' ') << "]";
      first_job = true;
      within_running_section = false;
   }

   os << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "}\n";
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_queue_section_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   if (parameter.show_ & QSTAT_DISPLAY_FULL) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"queues\": [\n";
      indent++;
   }
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_queue_section_finished(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   if (parameter.show_ & QSTAT_DISPLAY_FULL) {
      indent--;
      os << "\n" << std::string(indent * 3, ' ') << "]";
   }
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_queue_summary(std::ostream &os, const char *qname, queue_summary_t *summary,
                                                     QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   os << "\n" << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(qname);
   os << ",\n" << std::string(indent * 3, ' ') << "\"qtype\": " << raw2quotedJSON(summary->queue_type);
   os << ",\n" << std::string(indent * 3, ' ') << "\"slots_used\": " << summary->used_slots;
   os << ",\n" << std::string(indent * 3, ' ') << "\"slots_resv\": " << summary->resv_slots;
   os << ",\n" << std::string(indent * 3, ' ') << "\"slots_total\": " << summary->total_slots;
   if (summary->has_load_value && summary->has_load_value_from_object) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"load_avg\": " << summary->load_avg;
   } else {
      os << ",\n" << std::string(indent * 3, ' ') << "\"load_avg\": " << 0;
   }
   os << ",\n" << std::string(indent * 3, ' ') << "\"arch\": " << raw2quotedJSON(summary->arch);
   os << ",\n" << std::string(indent * 3, ' ') << "\"state\": " << raw2quotedJSON(summary->state);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_queue_started(std::ostream &os, const char *qname, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   if (parameter.show_ & QSTAT_DISPLAY_FULL) {
      within_queue_section = true;
      if (!first_queue) {
         os << ",\n";
      } else {
         first_queue = false;
      }
      os << std::string(indent * 3, ' ') << "{";
      indent++;
   }
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_queue_finished(std::ostream &os, const char *qname, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   if (parameter.show_ & QSTAT_DISPLAY_FULL) {
      os << "\n";
      indent--;
      os << std::string(indent * 3, ' ') << "}";
      within_queue_section = false;
   }
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_queue_load_alarm(std::ostream &os, const char *qname, const char *reason) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << "\"load_alarm_reason\": " << raw2quotedJSON(reason);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_queue_suspend_alarm(std::ostream &os, const char *qname, const char *reason) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << "\"suspend_alarm_reason\": " << raw2quotedJSON(reason);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_queue_message(std::ostream &os, const char *qname, const char *message) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << "\"message\": " << raw2quotedJSON(message);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_queue_resource_started(std::ostream &os, const char *name) {
   DENTER(TOP_LAYER);
   os << ",\n";
   os << std::string(indent * 3, ' ') << "\"queue_resources\": {";
   indent++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_queue_resource_finished(std::ostream &os, const char *name) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "}";
   first_sub_object = true;
   DRETURN_VOID;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_queue_resource(std::ostream &os, const lListElem *resource, const char *dom,
                                                      const char *name, const char *value, const char *details) {
   DENTER(TOP_LAYER);
   if (first_sub_object) {
      os << "\n";
      first_sub_object = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << raw2quotedJSON(name) << ": ";
   show_resource_as_JSON_type(os, resource);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_queue_jobs_started(std::ostream &os, const char *qname,
                                                          QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   if (within_running_section) {
      DRETURN_VOID;
   }
   if (parameter.show_ & QSTAT_DISPLAY_FULL) {
      if (first_queue) {
         os << "\n";
         first_queue = false;
      } else {
         os << ",\n";
      }
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"jobs_running\": [";
   indent++;
   if ((parameter.show_ & QSTAT_DISPLAY_FULL) == 0) {
      within_running_section = true;
   }

   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_queue_jobs_finished(std::ostream &os, const char *qname,
                                                           QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   if (within_running_section) {
      DRETURN_VOID;
   }

   os << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "]";
   first_job = true;
   DRETURN_VOID;
}


void ocs::QStatDefaultViewJSON::report_pending_jobs_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   if (within_running_section) {
      os << "\n";
      indent--;
      os << std::string(indent * 3, ' ') << "]";
      first_job = true;
      within_running_section = false;
   }

   if (parameter.show_ & QSTAT_DISPLAY_FULL) {
      if (first_queue) {
         os << "\n";
         first_queue = false;
      } else {
         os << ",\n";
      }
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"jobs_pending\": [";
   indent++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_pending_jobs_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "]";
   first_job = true;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_finished_jobs_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   if (parameter.show_ & QSTAT_DISPLAY_FULL) {
      if (first_queue) {
         os << "\n";
         first_queue = false;
      } else {
         os << ",\n";
      }
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"jobs_finished\": [";
   indent++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_finished_jobs_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "]";
   first_job = true;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_error_jobs_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   if (parameter.show_ & QSTAT_DISPLAY_FULL) {
      if (first_queue) {
         os << "\n";
         first_queue = false;
      } else {
         os << ",\n";
      }
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "\"jobs_error\": [";
   indent++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_error_jobs_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "]";
   first_job = true;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_job(std::ostream &os, const uint32_t jid, job_summary_t *summary,
                                           QStatParameter &parameter, QStatModelBase &model) {
   DENTER(TOP_LAYER);

   const bool show_ext = (parameter.show_ & QSTAT_DISPLAY_EXTENDED) ? true : false;
   const bool show_urg = (parameter.show_ & QSTAT_DISPLAY_URGENCY) ? true : false;
   const bool show_pri = (parameter.show_ & QSTAT_DISPLAY_PRIORITY) ? true : false;
   const bool show_tsk_ext = (parameter.show_ & QSTAT_DISPLAY_TASKS) ? true : false;
   const bool show_time = !show_ext | show_tsk_ext | show_urg | show_pri;

   if (first_job) {
      os << "\n";
      first_job = false;
   } else {
      os << ",\n";
   }

   os << std::string(indent * 3, ' ') << "{\n";
   indent++;

   os << std::string(indent * 3, ' ') << "\"job_number\": " << jid;
   os << ",\n" << std::string(indent * 3, ' ') << "\"prio\": " << summary->nprior;
   if (show_ext) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"ntckts\": " << summary->ntckts;
   }
   if (show_urg) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"nurg\": " << summary->nurg;
      os << ",\n" << std::string(indent * 3, ' ') << "\"urg\": " << summary->urg;
      os << ",\n" << std::string(indent * 3, ' ') << "\"rrcontr\": " << summary->rrcontr;
      os << ",\n" << std::string(indent * 3, ' ') << "\"wtcontr\": " << summary->wtcontr;
      os << ",\n" << std::string(indent * 3, ' ') << "\"dlcontr\": " << summary->dlcontr;
   }
   if (show_pri) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"nppri\": " << summary->nppri;
      os << ",\n" << std::string(indent * 3, ' ') << "\"priority\": " << summary->priority;
   }
   os << ",\n" << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(summary->name);
   os << ",\n" << std::string(indent * 3, ' ') << "\"owner\": " << raw2quotedJSON(summary->user);
   if (show_ext) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"project\": "
            << raw2quotedJSON(summary->project ? summary->project : "");
      os << ",\n" << std::string(indent * 3, ' ') << "\"department\": "
            << raw2quotedJSON(summary->department ? summary->department : "");
   }
   os << ",\n" << std::string(indent * 3, ' ') << "\"state\": " << raw2quotedJSON(summary->state);
   if (show_time) {
      const char *attrib = summary->is_running ? "start_time" : "submission_time";
      const uint64_t timestamp = summary->is_running ? summary->start_time : summary->submit_time;

      os << ",\n" << std::string(indent * 3, ' ') << raw2quotedJSON(attrib) << ": \"";
      show_ISO_8601_timestamp(os, timestamp);
      os << "\"";
   }
   if (show_urg && summary->deadline > 0) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"" << "deadline" << "\": \"";
      show_ISO_8601_timestamp(os, summary->deadline);
      os << "\"";
   }
   if (summary->is_running) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"queue_name\": " << raw2quotedJSON(summary->queue ? summary->queue : "");
   }
   if (show_ext) {
      if (summary->has_cpu_usage) {
         os << ",\n" << std::string(indent * 3, ' ') << "\"cpu_usage\": " << summary->cpu_usage;
      }
      if (summary->has_mem_usage) {
         os << ",\n" << std::string(indent * 3, ' ') << "\"mem_usage\": " << summary->mem_usage;
      }
      if (summary->has_io_usage) {
         os << ",\n" << std::string(indent * 3, ' ') << "\"io_usage\": " << summary->io_usage;
      }
      if (summary->is_queue_assigned) {
         os << ",\n" << std::string(indent * 3, ' ') << "\"tickets\": " << summary->tickets;
         os << ",\n" << std::string(indent * 3, ' ') << "\"override_tickets\": " << summary->override_tickets;
         os << ",\n" << std::string(indent * 3, ' ') << "\"jobshare\": " << summary->share;
         os << ",\n" << std::string(indent * 3, ' ') << "\"otickets\": " << summary->otickets;
         os << ",\n" << std::string(indent * 3, ' ') << "\"ftickets\": " << summary->ftickets;
         os << ",\n" << std::string(indent * 3, ' ') << "\"stickets\": " << summary->stickets;
      }
   }
   if ((parameter.group_opt_ & GROUP_NO_PETASK_GROUPS)) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"master\": "
            << raw2quotedJSON(summary->master ? summary->master : "");
   }
   os << ",\n" << std::string(indent * 3, ' ') << "\"slots\": " << summary->slots;
   if (summary->task_id && summary->is_array) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"tasks\": " << summary->task_id;
   }

   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_job_finished(std::ostream &os, u_int jid) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}


void ocs::QStatDefaultViewJSON::report_requested_pe(std::ostream &os, const char *pe_name, const char *pe_range) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << "\"requested_pe\": {";
   indent++;
   os << "\n" << std::string(indent * 3, ' ') << "\"pe_name\": " << raw2quotedJSON(pe_name ? pe_name : "");
   os << ",\n" << std::string(indent * 3, ' ') << "\"pe_range\": " << raw2quotedJSON(pe_range ? pe_range : "");
   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_granted_pe(std::ostream &os, const char *pe_name, const int pe_slots) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << "\"granted_pe\": {";
   indent++;
   os << "\n" << std::string(indent * 3, ' ') << "\"pe_name\": " << raw2quotedJSON(pe_name ? pe_name : "");
   os << ",\n" << std::string(indent * 3, ' ') << "\"slots\": " << pe_slots;
   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_hard_requested_queues_started(std::ostream &os, const int scope) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << "\"" << scope_to_string(scope) << "_hard_queue\": {";
   indent++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_hard_requested_queues_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "}";
   first_sub_object = true;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_hard_requested_queue(std::ostream &os, int scope, const char *name) {
   DENTER(TOP_LAYER);
   if (first_sub_object) {
      os << "\n";
      first_sub_object = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << raw2quotedJSON(name);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_soft_requested_queues_started(std::ostream &os, int scope) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << "\"" << scope_to_string(scope) << "_soft_queue\": {";
   indent++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_soft_requested_queues_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "}";
   first_sub_object = true;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_soft_requested_queue(std::ostream &os, int scope, const char *name) {
   DENTER(TOP_LAYER);
   if (first_sub_object) {
      os << "\n";
      first_sub_object = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << raw2quotedJSON(name);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_hard_resources_started(std::ostream &os, const int scope) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << "\"" << scope_to_string(scope) << "_hard_request\": [";
   indent++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_hard_resources_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   first_sub_object = true;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_hard_resource(std::ostream &os, int scope, const lListElem *resource,
                                                     const char *name, const char *value, double uc) {
   DENTER(TOP_LAYER);
   if (first_sub_object) {
      os << "\n";
      first_sub_object = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "{\n";
   indent++;
   os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(name) << ",\n";
   os << std::string(indent * 3, ' ') << "\"value\": ";
   show_resource_as_JSON_type(os, resource);
   os << ",\n";
   os << std::string(indent * 3, ' ') << "\"utilization_contribution\": " << uc << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_default_request_started(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << "\"" << "default_hard_request\": [";
   indent++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_default_request_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   first_sub_object = true;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_default_request(std::ostream &os, const char *name, const char *value) {
   DENTER(TOP_LAYER);
   if (first_sub_object) {
      os << "\n";
      first_sub_object = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "{\n";
   indent++;
   os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(name) << ",\n";
   // values does not need to get quoted because a default request is automatically a number
   os << std::string(indent * 3, ' ') << "\"value\": " << value << ",\n";
   os << std::string(indent * 3, ' ') << "\"is_default_request\": " << "true" << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}


void ocs::QStatDefaultViewJSON::report_soft_resources_started(std::ostream &os, int scope) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << "\"" << scope_to_string(scope) << "_soft_request\": [";
   indent++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_soft_resources_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   first_sub_object = true;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_soft_resource(std::ostream &os, int scope, const lListElem *resource,
                                                     const char *name, const char *value, double uc) {
   DENTER(TOP_LAYER);
   if (first_sub_object) {
      os << "\n";
      first_sub_object = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << "{\n";
   indent++;
   os << std::string(indent * 3, ' ') << "\"name\": " << raw2quotedJSON(name) << ",\n";
   os << std::string(indent * 3, ' ') << "\"value\": ";
   show_resource_as_JSON_type(os, resource);
   os << ",\n";
   os << std::string(indent * 3, ' ') << "\"utilization_contribution\": " << uc << "\n";
   indent--;
   os << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_predecessors_requested_started(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << "\"predecessors_requested\": [";
   indent++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_predecessors_requested_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   first_sub_object = true;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_predecessor_requested(std::ostream &os, const char *name) {
   DENTER(TOP_LAYER);
   if (first_sub_object) {
      os << "\n";
      first_sub_object = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << raw2quotedJSON(name);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_binding_started(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << "\"requested_binding\": {";
   indent++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_binding_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_binding(std::ostream &os, const char *binding) {
   DENTER(TOP_LAYER);
   // not implemented because parameters are shown individually
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_binding_attribute(std::ostream &os, const char *name, const char *value) {
   os << ",\n" << std::string(indent * 3, ' ') << "\"" << name << "\": " << raw2quotedJSON(value);
}

void ocs::QStatDefaultViewJSON::report_binding_attribute(std::ostream &os, const char *name, const uint32_t value) {
   os << ",\n" << std::string(indent * 3, ' ') << "\"" << name << "\": " << value;
}


void ocs::QStatDefaultViewJSON::report_predecessors_started(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << "\"predecessors\": {";
   indent++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_predecessors_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "}";
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_predecessor(std::ostream &os, const uint32_t jid) {
   DENTER(TOP_LAYER);
   if (first_sub_object) {
      os << "\n";
      first_sub_object = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << jid;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_ad_predecessors_requested_started(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << "\"ad_predecessors_requested\": [";
   indent++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_ad_predecessors_requested_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   first_sub_object = true;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_ad_predecessor_requested(std::ostream &os, const char *name) {
   DENTER(TOP_LAYER);
   if (first_sub_object) {
      os << "\n";
      first_sub_object = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << raw2quotedJSON(name);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_ad_predecessors_started(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << ",\n" << std::string(indent * 3, ' ') << "\"ad_predecessors_requested\": [";
   indent++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_ad_predecessors_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   indent--;
   os << "\n" << std::string(indent * 3, ' ') << "]";
   first_sub_object = true;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_ad_predecessor(std::ostream &os, uint32_t jid) {
   DENTER(TOP_LAYER);
   if (first_sub_object) {
      os << "\n";
      first_sub_object = false;
   } else {
      os << ",\n";
   }
   os << std::string(indent * 3, ' ') << jid;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_sub_tasks_started(std::ostream &os) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_sub_tasks_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewJSON::report_sub_task(std::ostream &os, task_summary_t *summary) {
   DENTER(TOP_LAYER);
   if (summary->task_id) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"task_id\": " << raw2quotedJSON(summary->task_id);
   }
   if (summary->has_cpu_usage) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"cpu_usage\": " << summary->cpu_usage;
   }
   if (summary->has_mem_usage) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"mem_usage\": " << summary->mem_usage;
   }
   if (summary->has_io_usage) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"io_usage\": " << summary->io_usage;
   }
   if (summary->has_exit_status) {
      os << ",\n" << std::string(indent * 3, ' ') << "\"stat\": " << summary->exit_status;
   }
   DRETURN_VOID;
}

void
ocs::QStatDefaultViewJSON::report_additional_info(std::ostream &os, job_additional_info_t name, const char *value) {
   DENTER(TOP_LAYER);
   // no need to show this again.
   DRETURN_VOID;
}
