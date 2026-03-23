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
#include <sstream>
#include <format>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_host.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_ulong.h"

#include "ocs_QStatDefaultViewPlain.h"
#include "msg_qstat.h"

#include "msg_clients_common.h"


inline void opti_print8(std::ostream& os, double value) {
   if (value > 99999999) {
      os << std::format("{:>8.3g} ", value);
   } else {
      os << std::format("{:>8.0f} ", value);
   }
}

#if 0
/* regular output */
static char jhul1[] = "---------------------------------------------------------------------------------------------";
/* -g t */
static char jhul2[] = "-";
/* -ext */
static char jhul3[] = "-------------------------------------------------------------------------------";
/* -t */
static char jhul4[] = "-----------------------------------------------------";
/* -urg */
static char jhul5[] = "----------------------------------------------------------------";
/* -pri */
static char jhul6[] = "-----------------------------------";
#endif

void ocs::QStatDefaultViewPlain::show_header_with_title(std::ostream &os, const QStatParameter &parameter, const char *title) {
   DENTER(TOP_LAYER);
   const bool sge_ext = (parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED);
   const auto line = std::string(sge_ext ? 165 : 81, '#');

   os << "\n" << line
      << "\n" << title
      << "\n" << line
      << "\n";

   last_job_id = 0;

   DRETURN_VOID;
}


void ocs::QStatDefaultViewPlain::report_started(std::ostream &os) {
}

void ocs::QStatDefaultViewPlain::report_finished(std::ostream &os) {
}

void ocs::QStatDefaultViewPlain::report_queue_summary(std::ostream &os, const char* qname, queue_summary_t *summary, QStatParameter &parameter)
{
   DENTER(TOP_LAYER);
   int sge_ext = parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED;

   if (!header_printed) {
      header_printed = true;

      os << std::format("{:<{}.{}} ", MSG_QSTAT_PRT_QUEUENAME, parameter.longest_queue_length, parameter.longest_queue_length)
         << std::format("{:<5.5} ", MSG_QSTAT_PRT_QTYPE)
         << std::format("{:<14.14} ", MSG_QSTAT_PRT_RESVUSEDTOT)
         << std::format("{:<8.8} ", summary->load_avg_str)
         << std::format("{:<13.13} ", LOAD_ATTR_ARCH)
         << std::format("{} ", MSG_QSTAT_PRT_STATES)
         << "\n";
   }

   os << "---------------------------------------------------------------------------------";
   if (sge_ext) {
      os << "------------------------------------------------------------------------------------------------------------";
   }
   for(int i = 0; i < parameter.longest_queue_length - 30; i++)
      os << "-";
   os << "\n";


   // queue name
   os << std::format("{:<{}.{}} ", qname, parameter.longest_queue_length, parameter.longest_queue_length);

   // queue type
   os << std::format("{:<5.5} ", summary->queue_type);

   /* number of used/total slots */
   std::ostringstream ss_res_used_total;
   ss_res_used_total << std::format("{}/{}/{}", summary->resv_slots, summary->used_slots, summary->total_slots);
   os << std::format("{:<14.14} ", ss_res_used_total.str());

   /* load avg */
   std::ostringstream ss_load_avg;
   // Why is has_load_value required?
   if (summary->has_load_value || (summary->state != nullptr && strchr(summary->state, 'u') != nullptr)) {
      ss_load_avg << "-NA-";
   } else {
      if (summary->has_load_value_from_object) {
         ss_load_avg << std::format("{:2.2f}", summary->load_avg);
      } else {
         ss_load_avg << "---";
      }
   }
   os << std::format("{:<8.8} ", ss_load_avg.str());

   /* arch */
   os << std::format("{:<13.13} ", summary->arch ? summary->arch : "-NA-");

   // state
   os << std::format("{} ", summary->state ? summary->state : "NA");

   os << "\n";

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_queue_started(std::ostream &os, const char* qname, QStatParameter &parameter) {
}

void ocs::QStatDefaultViewPlain::report_queue_jobs_started(std::ostream &os, const char *qname) {
}

void ocs::QStatDefaultViewPlain::report_queue_finished(std::ostream &os, const char *qname, QStatParameter &parameter) {
}

void ocs::QStatDefaultViewPlain::report_queue_jobs_finished(std::ostream &os, const char *qname, QStatParameter &parameter) {
}

void ocs::QStatDefaultViewPlain::report_queue_load_alarm(std::ostream &os, const char* qname, const char* reason)
{
   DENTER(TOP_LAYER);
   os << "\t" << (reason ? reason : "no alarm reason given") << "\n";
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_queue_suspend_alarm(std::ostream &os, const char* qname, const char* reason)
{
   DENTER(TOP_LAYER);
   os << "\t" << (reason ? reason : "no alarm reason given") << "\n";
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_queue_message(std::ostream &os, const char* qname, const char *message)
{
   DENTER(TOP_LAYER);
   os << "\t" << (message != nullptr ? message : "no queue message given") << "\n";
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_queue_resource(std::ostream &os, const char* dom,
                                       const char* name, const char* value, const char *details)
{
   DENTER(TOP_LAYER);
   os << "\t";
   if (details != nullptr && strlen(details) > 0) {
      os << std::format("{}:{}={} ({})", dom, name, value, details);
   } else {
      os << std::format("{}:{}={}", dom, name, value);
   }
   os << "\n";
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_pending_jobs_started(std::ostream &os, QStatParameter &parameter)
{
   DENTER(TOP_LAYER);

   static bool pending_header_printed = false;
   if (!pending_header_printed && (parameter.full_listing_ & QSTAT_DISPLAY_FULL) && (parameter.full_listing_ & QSTAT_DISPLAY_PENDING)) {
      show_header_with_title(os, parameter, MSG_QSTAT_PRT_PEDINGJOBS);
      pending_header_printed = true;
   }
   last_job_id = 0;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_pending_jobs_finished(std::ostream &os) {
}

void ocs::QStatDefaultViewPlain::report_finished_jobs_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   static bool finished_header_printed = false;
   if (!finished_header_printed) {
      show_header_with_title(os, parameter, MSG_QSTAT_PRT_JOBSWAITINGFORACCOUNTING);
      finished_header_printed = true;
   }
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_finished_jobs_finished(std::ostream &os) {
}

void ocs::QStatDefaultViewPlain::report_error_jobs_started(std::ostream &os, QStatParameter &parameter) {
   DENTER(TOP_LAYER);
   static bool error_header_printed = false;
   if (!error_header_printed) {
      show_header_with_title(os, parameter, MSG_QSTAT_PRT_ERRORJOBS);
   }
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_error_jobs_finished(std::ostream &os) {
}

void ocs::QStatDefaultViewPlain::report_job(std::ostream &os, u_long32 jid, job_summary_t *summary, QStatParameter &parameter, QStatGenericModel &model) {
   DENTER(TOP_LAYER);
   int sge_urg, sge_pri, sge_ext, sge_time, tsk_ext;
   bool print_job_id;

   dstring ds = DSTRING_INIT;

   sge_ext = ((parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED) == QSTAT_DISPLAY_EXTENDED);
   tsk_ext = (parameter.full_listing_ & QSTAT_DISPLAY_TASKS);
   sge_urg = (parameter.full_listing_ & QSTAT_DISPLAY_URGENCY);
   sge_pri = (parameter.full_listing_ & QSTAT_DISPLAY_PRIORITY);
   sge_time = !sge_ext;
   sge_time = sge_time | tsk_ext | sge_urg | sge_pri;

   if ((parameter.full_listing_ & QSTAT_DISPLAY_FULL) == QSTAT_DISPLAY_FULL) {
      job_header_printed = true;
   }

   print_job_id = summary->print_jobid;

   last_job_id = jid;
   if (summary->queue == nullptr) {
      sge_dstring_clear(&last_queue_name);
   } else {
      sge_dstring_copy_string(&last_queue_name, summary->queue);
   }

   if (!job_header_printed) {
      std::ostringstream oss;

      oss << std::format("{:<10} ", "job-ID");
      oss << std::format("{:<7} ", "prior");
      if (sge_pri || sge_urg) {
         oss << std::format("{:<7} ", "nurg");
      }
      if (sge_pri) {
         oss << std::format("{:<7} ", "npprior");
      }
      if (sge_pri || sge_ext) {
         oss << std::format("{:<7} ", "ntckts");
      }
      if (sge_urg) {
         oss << std::format("{:<8} ", "urg");
         oss << std::format("{:<8} ", "rrcontr");
         oss << std::format("{:<8} ", "wtcontr");
         oss << std::format("{:<8} ", "dlcontr");
      }
      if (sge_pri) {
         oss << std::format("{:<5} ", "pri");
      }
      oss << std::format("{:<10} ", "name");
      oss << std::format("{:<12} ", "user");
      if (sge_ext) {
         oss << std::format("{:<16} ", "project");
         oss << std::format("{:<10} ", "department");
      }
      oss << std::format("{:<5} ", "state");
      if (sge_time) {
         oss << std::format("{:<19} ", "submit/start at");
      }
      if (sge_urg) {
         oss << std::format("{:<19} ", "deadline");
      }
      if (sge_ext) {
         oss << std::format("{:<10} ", "cpu");
         oss << std::format("{:<7} ", "mem");
         oss << std::format("{:<7} ", "io");
      }
      if (sge_ext) {
         oss << std::format("{:<5} ", "tckts");
         oss << std::format("{:<5} ", "ovrts");
         oss << std::format("{:<5} ", "otckt");
         oss << std::format("{:<5} ", "ftckt");
         oss << std::format("{:<5} ", "stckt");
         oss << std::format("{:<5} ", "share");
      }
      oss << std::format("{:<{}} ", "queue", parameter.longest_queue_length);
      oss << std::format("{:<6} ", (parameter.group_opt_ & GROUP_NO_PETASK_GROUPS) ? "master" : "slots");
      oss << std::format("{:<10} ", "ja-task-ID");
      if (tsk_ext) {
         oss << std::format("{:<16} ", "task-ID");
         oss << std::format("{:<5} ", "state");
         oss << std::format("{:<10} ", "cpu");
         oss << std::format("{:<7} ", "mem");
         oss << std::format("{:<7} ", "io");
         oss << std::format("{:<4} ", "stat");
      }

      size_t line_length = oss.str().length();

      os << oss.str() << "\n";
      os << std::string(line_length, '-') << "\n";

      job_header_printed = true;
   }

   /* job id */
   /* job number / ja task id */
   if (print_job_id) {
      os << std::format("{:>10} ", jid);
   } else {
      os << std::format("{:>10} ", "");
   }

   if (print_job_id) {
      os << std::format("{:>7.5f} ", summary->nprior); /* nprio 0.0 - 1.0 */
   } else {
      os << std::string(7 + 1, ' ');
   }
   if (sge_pri || sge_urg) {
      if (print_job_id) {
         os << std::format("{:>7.5f} ", summary->nurg); /* nurg 0.0 - 1.0 */
      } else {
         os << std::string(7 + 1, ' ');
      }
   }
   if (sge_pri) {
      if (print_job_id) {
         os << std::format("{:>7.5f} ", summary->nppri); /* nppri 0.0 - 1.0 */
      } else {
         os << std::string(7 + 1, ' ');
      }
   }
   if (sge_pri || sge_ext) {
      if (print_job_id) {
         os << std::format("{:>7.5f} ", summary->ntckts); /* ntix 0.0 - 1.0 */
      } else {
         os << std::string(7 + 1, ' ');
      }
   }
   if (sge_urg) {
      if (print_job_id) {
         opti_print8(os, summary->urg);
         opti_print8(os, summary->rrcontr);
         opti_print8(os, summary->wtcontr);
         opti_print8(os, summary->dlcontr);
      } else {
         os << std::string((8 + 1) * 4, ' ');
      }
   }
   if (sge_pri) {
      if (print_job_id) {
         os << std::format("{:>5d} ", (int)summary->priority);
      } else {
         os << std::string(5 + 1, ' ');
      }
   }

   if (print_job_id) {
      os << std::format("{:<10.10} ", summary->name);
      os << std::format("{:<12.12} ", summary->user);
   } else {
      os << std::string(10 + 1 + 12 + 1, ' ');
   }

   if (sge_ext) {
      if (print_job_id) {
         os << std::format("{:<16.16} ", summary->project?summary->project:"NA");
         os << std::format("{:<10.10} ", summary->department?summary->department:"NA");
      } else {
         os << std::string(16 + 1 + 10 + 1, ' ');
      }
   }

   if (print_job_id) {
      os << std::format("{:<5.5} ", summary->state);
   } else {
      os << std::string(5 + 1, ' ');
   }

   if (sge_time) {
      if (print_job_id) {
         if (summary->is_running) {
            os << std::format("{:<19} ", sge_ctime64_short(summary->start_time, &ds));
         } else {
            os << std::format("{:<19} ", sge_ctime64_short(summary->submit_time, &ds));
         }
      } else {
         os << std::string(19 + 1, ' ');
      }
   }

   if (sge_urg) {
      if (print_job_id) {
         if (summary->deadline) {
            os << std::format("{:<19} ", sge_ctime64_short(summary->deadline, &ds));
         } else {
            os << std::string(19 + 1, ' ');
         }
      } else {
         os << std::string(19 + 1, ' ');
      }
   }

   if (sge_ext) {
      /* scaled cpu usage */
      if (!summary->has_cpu_usage)
         os << std::format("{:<10.10} ", summary->is_running ? "NA" : "");
      else {
         int secs, minutes, hours, days;

         secs = summary->cpu_usage;

         days    = secs/(60*60*24);
         secs   -= days*(60*60*24);

         hours   = secs/(60*60);
         secs   -= hours*(60*60);

         minutes = secs/60;
         secs   -= minutes*60;

         os << std::format("{}:{:02}:{:02}:{:02} ", days, hours, minutes, secs);
      }
      /* scaled mem usage */
      if (summary->has_mem_usage) {
         os << std::format("{:<5.5f} ", summary->mem_usage);
      } else {
         os << std::format("{:<7.7} ", summary->is_running?"NA":"");
      }

      /* scaled io usage */
      if (summary->has_io_usage) {
         os << std::format("{:<5.5f} ", summary->io_usage);
      } else {
         os << std::format("{:<7.7} ", summary->is_running ? "NA" : "");
      }

      /* report jobs dynamic scheduling attributes */
      /* only scheduled have these attribute */
      /* Pending jobs can also have tickets */
      if (sge_ext || summary->is_queue_assigned) {
         os << std::format("{:<5d} ", (int)summary->tickets);
         os << std::format("{:<5d} ", (int)summary->override_tickets);
         os << std::format("{:<5d} ", (int)summary->otickets);
         os << std::format("{:<5d} ", (int)summary->ftickets);
         os << std::format("{:<5d} ", (int)summary->stickets);
         os << std::format("{:<5.2f} ", summary->share);
      } else {
         os << std::string((5 + 1) * 6, ' ');
      }
   }

   // queue
   if (!(parameter.full_listing_ & QSTAT_DISPLAY_FULL)) {
      os << std::format("{:<{}.{}} ", summary->queue ? summary->queue : "",
                        parameter.longest_queue_length, parameter.longest_queue_length);
   }

   // master/slave or grantes slots
   if ((parameter.group_opt_ & GROUP_NO_PETASK_GROUPS)) {
      if (summary->master)
         os << std::format("{:<6} ", summary->master);
      else
         os << std::string(6 + 1, ' ');
   } else {
      os << std::format("{:<6d} ", (int)summary->slots);
   }

   // job array task id(s)
   if (summary->task_id && summary->is_array)
      os << std::format("{:<10s} ", summary->task_id ? summary->task_id : "");
   else
      os << std::string(10 + 1, ' ');

   if (!tsk_ext) {
      os << "\n";
   }

   sge_dstring_free(&ds);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_sub_tasks_started(std::ostream &os)
{
   DENTER(TOP_LAYER);

   sub_task_count = 0;

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_sub_task(std::ostream &os, task_summary_t *summary) {
   DENTER(TOP_LAYER);

   os << std::format("{:>16} ", summary->task_id ? summary->task_id : "x");
   os << std::format("{:>5.5} ", summary->state);

   if (summary->has_cpu_usage) {
      dstring resource_string = DSTRING_INIT;

      double_print_time_to_dstring(summary->cpu_usage, &resource_string, true);
      os << std::format("{:>10.10} ", sge_dstring_get_string(&resource_string));
      sge_dstring_free(&resource_string);
   } else {
      os << std::format("{:>10.10} ", summary->is_running? "NA" : "");
   }
   if (summary->has_mem_usage) {
      os << std::format("{:>5.5f} ", summary->mem_usage);
   } else {
      os << std::format("{:>7.7} ", summary->is_running? "NA" : "");
   }

   /* scaled io usage */
   if (summary->has_io_usage) {
      os << std::format("{:>5.5f} ", summary->io_usage);
   } else {
      os << std::format("{:>7.7} ", summary->is_running ? "NA" : "");
   }

   if (summary->has_exit_status) {
      os << std::format("{:>4d} ", (int)summary->exit_status);
   }

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_sub_tasks_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "\n";
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::show_header_with_subtitle(std::ostream &os, job_additional_info_t subtitle, const char *name, const char *value) {
   DENTER(TOP_LAYER);

   const char *name_str = nullptr;
   switch (subtitle) {
      case CHECKPOINT_ENV:
         name_str = "Checkpoint Env.:";
         break;
      case MASTER_QUEUE:
         name_str = "Master Queue:";
         break;
      case FULL_JOB_NAME:
         name_str = "Full jobname:";
         break;
      case REQUESTED_PE:
         name_str = "Requested PE:";
         break;
      case GRANTED_PE:
         name_str = "Granted PE:";
         break;
      case JOB_ADDITIONAL_INFO_ERROR:
         name_str = "Error:";
         break;
   }

   // Show the name of the additional info (e.g. "Checkpoint Env.:") and the name.
   os << std::format("\t{:<36.36} {} ", name_str != nullptr ? name_str : "", name != nullptr ? name : "");

   os << (value != nullptr ? value : "") << '\n';
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_requested_pe(std::ostream &os, const char *pe_name, const char *pe_range) {
   DENTER(TOP_LAYER);
   show_header_with_subtitle(os, REQUESTED_PE, pe_name, pe_range);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_granted_pe(std::ostream &os, const char* pe_name, int pe_slots) {
   DENTER(TOP_LAYER);
   std::ostringstream ss_slots;
   ss_slots << pe_slots;
   show_header_with_subtitle(os, GRANTED_PE, pe_name, ss_slots.str().c_str());
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_additional_info(std::ostream &os, job_additional_info_t name, const char *value) {
   DENTER(TOP_LAYER);
   show_header_with_subtitle(os, name, value, nullptr);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_request(std::ostream &os, const char *name, const char *value) {
   DENTER(TOP_LAYER);
   os << std::format("\t{}={} (default)\n", name, value);
   DRETURN_VOID;
}


void ocs::QStatDefaultViewPlain::report_hard_requested_queue(std::ostream &os, int scope, const char *name) {
   DENTER(TOP_LAYER);

   if (hard_requested_queue_count > 0) {
      os << ", " << name;
   } else {
      os << name;
   }
   hard_requested_queue_count++;

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_hard_requested_queues_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "\n";
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::show_queues_or_resource_started(std::ostream &os, int scope, bool queue, bool hard) {
   DENTER(TOP_LAYER);

   switch(scope) {
      case JRS_SCOPE_MASTER:
         if (queue) {
            if (hard) {
               os << std::format("\t{:<36.36} ", "Master task hard requested queues:");
            } else {
               os << std::format("\t{:<36.36} ", "Master task soft requested queues:");
            }
         } else {
            if (hard) {
               os << std::format("\t{:<36.36} ", "Master Hard Resources:");
            } else {
               os << std::format("\t{:<36.36} ", "Master Soft Resources:");
            }
         }
         break;
      case JRS_SCOPE_SLAVE:
         if (queue) {
            if (hard) {
               os << std::format("\t{:<36.36} ", "Slave task hard requested queues:");
            } else {
               os << std::format("\t{:<36.36} ", "Slave task soft requested queues:");
            }
         } else {
            if (hard) {
               os << std::format("\t{:<36.36} ", "Slave Hard Resources:");
            } else {
               os << std::format("\t{:<36.36} ", "Slave Soft Resources:");

            }
         }
         break;
      case JRS_SCOPE_GLOBAL:
         if (queue) {
            if (hard) {
               os << std::format("\t{:<36.36} ", "Hard requested queues:");
            } else {
               os << std::format("\t{:<36.36} ", "Soft requested queues:");
            }
         } else {
            if (hard) {
               os << std::format("\t{:<36.36} ", "Hard Resources:");
            } else {
               os << std::format("\t{:<36.36} ", "Soft Resources:");

            }
         }
      default:
         break;
   }

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_hard_requested_queues_started(std::ostream &os, int scope) {
   DENTER(TOP_LAYER);
   show_queues_or_resource_started(os, scope,true,true);
   hard_requested_queue_count = 0;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_soft_requested_queues_started(std::ostream &os, int scope) {
   DENTER(TOP_LAYER);

   show_queues_or_resource_started(os, scope, true, false);
   soft_requested_queue_count = 0;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_hard_resources_started(std::ostream &os, int scope) {
   DENTER(TOP_LAYER);
   show_queues_or_resource_started(os, scope, false, true);
   hard_resource_count = 0;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_soft_resources_started(std::ostream &os, int scope) {
   DENTER(TOP_LAYER);
   show_queues_or_resource_started(os, scope, false, false);
   soft_resource_count = 0;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_soft_requested_queue(std::ostream &os, int scope, const char *name) {
   DENTER(TOP_LAYER);

   if (soft_requested_queue_count > 0) {
      os << ", " << name;
   } else {
      os << name;
   }
   soft_requested_queue_count++;

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_soft_requested_queues_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "\n";
   DRETURN_VOID;
}



void ocs::QStatDefaultViewPlain::report_hard_resource(std::ostream &os, int scope, const char *name, const char *value, double uc) {
   DENTER(TOP_LAYER);
   if (hard_resource_count > 0 ) {
      os << std::format("\t{:<36.36} ", " ");
   }
   os << std::format("{}={} ({:f})\n", name, (value ? value : ""), uc);
   hard_resource_count++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_hard_resources_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (hard_resource_count == 0) {
      os << "\n";
   }
   DRETURN_VOID;
}


void ocs::QStatDefaultViewPlain::report_soft_resource(std::ostream &os, int scope, const char *name, const char *value, double uc) {
   DENTER(TOP_LAYER);
   if (soft_resource_count > 0 ) {
      os << std::format("\t{:<36.36} ", " ");
   }
   os << std::format("{}={}\n", name, value ? value : "");
   soft_resource_count++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_soft_resources_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (soft_resource_count == 0) {
      os << "\n";
   }
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_predecessors_requested_started(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << std::format("\t{:<36.36} ", "Predecessor Jobs (request):");
   predecessor_requested_count = 0;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_predecessor_requested(std::ostream &os, const char *name) {
   DENTER(TOP_LAYER);
   if (predecessor_requested_count > 0 ) {
      os << ", " << name;
   } else {
      os << name;
   }
   predecessor_requested_count++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_predecessors_requested_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "\n";
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_predecessors_started(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << std::format("\t{:<36.36} ", "Predecessor Jobs:");
   predecessor_count = 0;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_predecessor(std::ostream &os, u_long32 jid) {
   DENTER(TOP_LAYER);
   if (predecessor_count > 0 ) {
      os << ", " << jid;
   } else {
      os << jid;
   }
   predecessor_count++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_predecessors_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "\n";
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_ad_predecessors_requested_started(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << std::format("\t{:<36.36} ", "Predecessor Array Jobs (request):");
   ad_predecessor_requested_count = 0;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_ad_predecessor_requested(std::ostream &os, const char *name) {
   DENTER(TOP_LAYER);
   if (ad_predecessor_requested_count > 0) {
      os << ", " << name;
   } else {
      os << name;
   }
   ad_predecessor_requested_count++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_ad_predecessors_requested_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "\n";
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_ad_predecessors_started(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << std::format("\t{:<36.36} ", "Predecessor Array Jobs:");
   ad_predecessor_count = 0;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_ad_predecessor(std::ostream &os, u_long32 jid) {
   DENTER(TOP_LAYER);
   if (ad_predecessor_count > 0) {
      os << ", " << jid;
   } else {
      os << jid;
   }
   ad_predecessor_count++;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_ad_predecessors_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "\n";
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_binding_started(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << std::format("\t{:<36.36} ", "Binding:");
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_binding(std::ostream &os, const char *binding) {
   DENTER(TOP_LAYER);
   os << binding;
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_binding_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "\n";
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_job_finished(std::ostream &os, u_int jid) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}
