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

#include "uti/sge_rmon_macros.h"
#include "uti/ocs_TerminationManager.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_host.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_ulong.h"

#include "ocs_QStatDefaultViewPlain.h"
#include "msg_clients_common.h"
#include "msg_qstat.h"

#define OPTI_PRINT8(job_output, hide_data, value) \
if (value > 99999999 ) \
if (hide_data) { \
sge_dstring_sprintf_append(job_output, "%8s ", "*"); \
} else { \
sge_dstring_sprintf_append(job_output, "%8.3g ", value); \
} \
else  \
if (hide_data) { \
sge_dstring_sprintf_append(job_output, "%8s ", "*"); \
} else { \
sge_dstring_sprintf_append(job_output, "%8.0f ", value); \
} \

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

void ocs::QStatDefaultViewPlain::report_started(std::ostream &os) {
}

void ocs::QStatDefaultViewPlain::report_finished(std::ostream &os) {
}

void ocs::QStatDefaultViewPlain::report_queue_summary(std::ostream &os, const char* qname, queue_summary_t *summary, QStatParameter &parameter)
{
   DENTER(TOP_LAYER);
   int sge_ext = parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED;
   char to_print[80];
   dstring queue_output = DSTRING_INIT;

   if (!header_printed) {
      char temp[20];
      header_printed = true;

      snprintf(temp, sizeof(temp), "%%-%d.%ds", parameter.longest_queue_length, parameter.longest_queue_length);

      printf(temp,MSG_QSTAT_PRT_QUEUENAME);

      printf(" %-5.5s %-14.14s %-8.8s %-13.13s %s\n",
            MSG_QSTAT_PRT_QTYPE,
            MSG_QSTAT_PRT_RESVUSEDTOT,
            summary->load_avg_str,
            LOAD_ATTR_ARCH,
            MSG_QSTAT_PRT_STATES);
   }

   printf("---------------------------------------------------------------------------------%s",
      sge_ext?"------------------------------------------------------------------------------------------------------------":"");

   {
      int i;
      for(i=0; i< parameter.longest_queue_length - 30; i++)
         printf("-");
      printf("\n");
   }


   // queue name
   char temp[20];
   snprintf(temp, sizeof(temp), "%%-%d.%ds ", parameter.longest_queue_length, parameter.longest_queue_length);
   sge_dstring_sprintf_append(&queue_output, temp, qname);

   // queue type
   sge_dstring_sprintf_append(&queue_output, "%-5.5s ", summary->queue_type);

   /* number of used/total slots */
   dstring res_used_total = DSTRING_INIT;
   sge_dstring_sprintf_append(&res_used_total, "%d/%d/%d ", (int)summary->resv_slots, (int)summary->used_slots, (int)summary->total_slots);
   sge_dstring_sprintf_append(&queue_output, "%-14.14s ", sge_dstring_get_string(&res_used_total));
   sge_dstring_free(&res_used_total);

   /* load avg */
   dstring load_avg = DSTRING_INIT;
   // Why is has_load_value required?
   if (summary->has_load_value || (summary->state != nullptr && strchr(summary->state, 'u') != nullptr)) {
      sge_dstring_sprintf_append(&load_avg, "-NA- ");
   } else {
      if (summary->has_load_value_from_object) {
         sge_dstring_sprintf_append(&load_avg,"%2.2f ", summary->load_avg);
      } else {
         sge_dstring_sprintf_append(&load_avg,"---  ");
      }
   }
   sge_dstring_sprintf_append(&queue_output, "%-8.8s ", sge_dstring_get_string(&load_avg));
   sge_dstring_free(&load_avg);

   /* arch */
   dstring arch = DSTRING_INIT;
   if (summary->arch != nullptr) {
      sge_dstring_sprintf_append(&arch, "%s ", summary->arch);
   } else {
      snprintf(to_print, sizeof(to_print), "-NA- ");
   }
   sge_dstring_sprintf_append(&queue_output, "%-13.13s ", sge_dstring_get_string(&arch));
   sge_dstring_free(&arch);

   // state
   sge_dstring_sprintf_append(&queue_output, "%s\n", summary->state ? summary->state : "NA");
   printf("%s", sge_dstring_get_string(&queue_output));
   sge_dstring_free(&queue_output);

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
   printf("\t%s\n", reason != nullptr ? reason : "no alarm reason given");
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_queue_suspend_alarm(std::ostream &os, const char* qname, const char* reason)
{
   DENTER(TOP_LAYER);
   printf("\t%s\n", reason != nullptr ? reason : "no alarm reason given");
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_queue_message(std::ostream &os, const char* qname, const char *message)
{
   DENTER(TOP_LAYER);
   printf("\t%s\n", message != nullptr ? message : "no queue message given");
   DRETURN_VOID;
}


void ocs::QStatDefaultViewPlain::report_queue_resource(std::ostream &os, const char* dom,
                                       const char* name, const char* value, const char *details)
{
   DENTER(TOP_LAYER);
   if (details != nullptr && strlen(details) > 0) {
      printf("\t%s:%s=%s (%s)\n", dom, name, value, details);
   } else {
      printf("\t%s:%s=%s\n", dom, name, value);
   }
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_pending_jobs_started(std::ostream &os, QStatParameter &parameter)
{
   DENTER(TOP_LAYER);

   last_job_id = 0;
   sge_printf_header((parameter.full_listing_ & QSTAT_DISPLAY_FULL) |
                     (parameter.full_listing_ & QSTAT_DISPLAY_PENDING),
                     (parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED) == QSTAT_DISPLAY_EXTENDED);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_pending_jobs_finished(std::ostream &os) {
}

static char hashes[] = "##############################################################################################################";

void ocs::QStatDefaultViewPlain::report_finished_jobs_started(std::ostream &os, QStatParameter &parameter)
{
   int sge_ext = (parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED);

   DENTER(TOP_LAYER);

   last_job_id = 0;

   printf("\n################################################################################%s\n", sge_ext?hashes:"");
   printf("%s\n", MSG_QSTAT_PRT_JOBSWAITINGFORACCOUNTING);
   printf(  "################################################################################%s\n", sge_ext?hashes:"");

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_finished_jobs_finished(std::ostream &os) {
}

void ocs::QStatDefaultViewPlain::report_error_jobs_started(std::ostream &os, QStatParameter &parameter)
{
   int sge_ext = (parameter.full_listing_ & QSTAT_DISPLAY_EXTENDED);

   DENTER(TOP_LAYER);

   printf("\n################################################################################%s\n", sge_ext?hashes:"");
   printf("%s\n", MSG_QSTAT_PRT_ERRORJOBS);
   printf(  "################################################################################%s\n", sge_ext?hashes:"");

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_error_jobs_finished(std::ostream &os) {
}

void ocs::QStatDefaultViewPlain::report_zombie_jobs_started(std::ostream &os) {
}

void ocs::QStatDefaultViewPlain::report_zombie_jobs_finished(std::ostream &os) {
}

void ocs::QStatDefaultViewPlain::report_job(std::ostream &os, u_long32 jid, job_summary_t *summary, QStatParameter &parameter, QStatGenericModel &model)
{
   DENTER(TOP_LAYER);
   const char* indent = "";
   int sge_urg, sge_pri, sge_ext, sge_time, tsk_ext;
   bool print_job_id;

   bool hide_data = !job_is_visible(summary->user, model.is_manager_);
   if (hide_data) {
      return;
   }

   dstring ds = DSTRING_INIT;
   dstring job_output = DSTRING_INIT;

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
      int i;
      int line_length = parameter.longest_queue_length-10+1;
      char * seperator = sge_malloc(line_length);
      const char *part1 = "%s%-10.10s %s %s%s%s%s%s %-10.10s %-12.12s %s%-5.5s %s%s%s%s%s%s%s%s%s%-";
      const char *part3 = ".";
	   const char *part5 = "s %s %s%s%s%s%s%s";
      size_t part6_size = strlen(part1) + strlen(part3) + strlen(part5) + 20;
		char *part6 = sge_malloc(part6_size);

      job_header_printed = true;

      for (i=0; i<line_length; i++) {
         seperator[i] = '-';
      }
      seperator[line_length-1] = '\0';
      snprintf(part6, part6_size, "%s%d%s%d%s", part1, parameter.longest_queue_length, part3, parameter.longest_queue_length, part5);

      printf(part6, indent, "job-ID", "prior ",
            (sge_pri||sge_urg)?" nurg   ":"",
            sge_pri?" npprior":"",
            (sge_pri||sge_ext)?" ntckts ":"",
            sge_urg?" urg      rrcontr  wtcontr  dlcontr ":"",
            sge_pri?"  ppri":"",
               "name",
               "user",
            sge_ext?"project          department ":"",
               "state",
            sge_time?"submit/start at     ":"",
            sge_urg?" deadline           " : "",
            sge_ext?USAGE_ATTR_CPU "        " USAGE_ATTR_MEM "     " USAGE_ATTR_IO "      " : "",
            sge_ext?"tckts ":"",
            sge_ext?"ovrts ":"",
            sge_ext?"otckt ":"",
            sge_ext?"ftckt ":"",
            sge_ext?"stckt ":"",
            sge_ext?"share ":"",
               "queue",
            (parameter.group_opt_ & GROUP_NO_PETASK_GROUPS)?"master":"slots",
               "ja-task-ID ",
            tsk_ext?"task-ID ":"",
            tsk_ext?"state ":"",
            tsk_ext?USAGE_ATTR_CPU "        " USAGE_ATTR_MEM "     " USAGE_ATTR_IO "      " : "",
            tsk_ext?"stat ":"",
            tsk_ext?"failed ":"" );

      printf("\n%s%s%s%s%s%s%s%s\n", indent,
            jhul1,
            seperator,
            (parameter.group_opt_ & GROUP_NO_PETASK_GROUPS)?jhul2:"",
            sge_ext ? jhul3 : "",
            tsk_ext ? jhul4 : "",
            sge_urg ? jhul5 : "",
            sge_pri ? jhul6 : "");

      sge_free(&part6);
      sge_free(&seperator);
   }

   if (summary->is_zombie) {
      sge_printf_header(parameter.full_listing_ &
                        (QSTAT_DISPLAY_ZOMBIES | QSTAT_DISPLAY_FULL),
                        sge_ext);
   }


   /* job id */
   /* job number / ja task id */
   if (print_job_id) {
      if (hide_data) {
         // maximum job id is U_LONG32_MAX = 4294967295 = 10 digits
         sge_dstring_sprintf_append(&job_output, "%10s ", "*");
      } else {
         sge_dstring_sprintf_append(&job_output, "%10" sge_fuu32 " ", jid);
      }
   } else {
      sge_dstring_sprintf_append(&job_output, "           ");
   }

   if (print_job_id) {
      if (hide_data) {
         sge_dstring_sprintf_append(&job_output, "%7s ", "*");
      } else {
         sge_dstring_sprintf_append(&job_output, "%7.5f ", summary->nprior); /* nprio 0.0 - 1.0 */
      }
   } else {
      sge_dstring_sprintf_append(&job_output,"        ");
   }
   if (sge_pri || sge_urg) {
      if (print_job_id)
         if (hide_data) {
            sge_dstring_sprintf_append(&job_output,"%7s ", "*");
         } else {
            sge_dstring_sprintf_append(&job_output,"%7.5f ", summary->nurg); /* nurg 0.0 - 1.0 */
         }
      else
         sge_dstring_sprintf_append(&job_output,"        ");
   }
   if (sge_pri) {
      if (print_job_id)
         if (hide_data) {
            sge_dstring_sprintf_append(&job_output,"%7s ", "*");
         } else {
            sge_dstring_sprintf_append(&job_output,"%7.5f ", summary->nppri); /* nppri 0.0 - 1.0 */
         }
      else
         sge_dstring_sprintf_append(&job_output,"        ");
   }
   if (sge_pri || sge_ext) {
      if (print_job_id)
         if (hide_data) {
            sge_dstring_sprintf_append(&job_output,"%7s ", "*");
         } else {
            sge_dstring_sprintf_append(&job_output,"%7.5f ", summary->ntckts); /* ntix 0.0 - 1.0 */
         }
      else
         sge_dstring_sprintf_append(&job_output,"        ");
   }

   if (sge_urg) {
      if (print_job_id) {
         OPTI_PRINT8(&job_output, hide_data, summary->urg);
         OPTI_PRINT8(&job_output, hide_data, summary->rrcontr);
         OPTI_PRINT8(&job_output, hide_data, summary->wtcontr);
         OPTI_PRINT8(&job_output, hide_data, summary->dlcontr);
      } else {
         sge_dstring_sprintf_append(&job_output,"                                    ");
      }
   }

   if (sge_pri) {
      if (print_job_id) {
         if (hide_data) {
            sge_dstring_sprintf_append(&job_output,"%5s ", "*");
         } else {
            sge_dstring_sprintf_append(&job_output,"%5d ", (int)summary->priority);
         }
      } else {
         sge_dstring_sprintf_append(&job_output,"                  ");
      }
   }

   if (print_job_id) {
      /* job name */
      if (hide_data) {
         sge_dstring_sprintf_append(&job_output,"%-10s ", "*");
      } else {
         sge_dstring_sprintf_append(&job_output,"%-10.10s ", summary->name);
      }

      /* job owner */
      if (hide_data) {
         sge_dstring_sprintf_append(&job_output,"%-12s ", "*");
      } else {
         sge_dstring_sprintf_append(&job_output,"%-12.12s ", summary->user);
      }
   } else {
      sge_dstring_sprintf_append(&job_output,"                        ");
   }

   if (sge_ext) {
      if (print_job_id) {
         if (hide_data) {
            sge_dstring_sprintf_append(&job_output,"%-16s ", "*");
            sge_dstring_sprintf_append(&job_output,"%-10s ", "*");
         } else {
            /* job project */
            sge_dstring_sprintf_append(&job_output,"%-16.16s ", summary->project?summary->project:"NA");
            /* job department */
            sge_dstring_sprintf_append(&job_output,"%-10.10s ", summary->department?summary->department:"NA");
         }
      } else {
         sge_dstring_sprintf_append(&job_output,"                            ");
      }
   }

   if (print_job_id) {
      if (hide_data) {
         sge_dstring_sprintf_append(&job_output,"%-5s ", "*");
      } else {
         sge_dstring_sprintf_append(&job_output,"%-5.5s ", summary->state);
      }
   } else {
      sge_dstring_sprintf_append(&job_output,"      ");
   }

   if (sge_time) {
      if (print_job_id) {
         /* start/submit time */
         if (summary->is_running) {
            if (hide_data) {
               sge_dstring_sprintf_append(&job_output,"%-19s ", "*");
            } else {
               sge_dstring_sprintf_append(&job_output,"%s ", sge_ctime64_short(summary->start_time, &ds));
            }
         } else {
            if (hide_data) {
               sge_dstring_sprintf_append(&job_output,"%-19s ", "*");
            } else {
               sge_dstring_sprintf_append(&job_output,"%s ", sge_ctime64_short(summary->submit_time, &ds));
            }
         }
      } else {
         sge_dstring_sprintf_append(&job_output,"                    ");
      }
   }

   /* deadline time */
   if (sge_urg) {
      if (print_job_id) {
         if (summary->deadline )
            if (hide_data) {
               sge_dstring_sprintf_append(&job_output,"%-19s ", "*");
            } else {
               sge_dstring_sprintf_append(&job_output,"%s ", sge_ctime64_short(summary->deadline, &ds));
            }
         else
            sge_dstring_sprintf_append(&job_output,"                    ");
      } else {
         sge_dstring_sprintf_append(&job_output,"                    ");
      }
   }

   if (sge_ext) {
      /* scaled cpu usage */
      if (!summary->has_cpu_usage)
         if (hide_data) {
            sge_dstring_sprintf_append(&job_output,"%-10s ", "*");
         } else {
            sge_dstring_sprintf_append(&job_output,"%-10.10s ", summary->is_running?"NA":"");
         }
      else {
         int secs, minutes, hours, days;

         secs = summary->cpu_usage;

         days    = secs/(60*60*24);
         secs   -= days*(60*60*24);

         hours   = secs/(60*60);
         secs   -= hours*(60*60);

         minutes = secs/60;
         secs   -= minutes*60;

         if (hide_data) {
            sge_dstring_sprintf_append(&job_output,"%-10s ", "*");
         } else {
            sge_dstring_sprintf_append(&job_output,"%d:%2.2d:%2.2d:%2.2d ", days, hours, minutes, secs);
         }
      }
      /* scaled mem usage */
      if (!summary->has_mem_usage)
         if (hide_data) {
            sge_dstring_sprintf_append(&job_output,"%-7s ", "*");
         } else {
            sge_dstring_sprintf_append(&job_output,"%-7.7s ", summary->is_running?"NA":"");
         }
      else
         if (hide_data) {
            sge_dstring_sprintf_append(&job_output,"%-5s ", "*");
         } else {
            sge_dstring_sprintf_append(&job_output,"%-5.5f ", summary->mem_usage);
         }

      /* scaled io usage */
      if (!summary->has_io_usage)
         if (hide_data) {
            sge_dstring_sprintf_append(&job_output,"%-7s ", "*");
         } else {
            sge_dstring_sprintf_append(&job_output,"%-7.7s ", summary->is_running?"NA":"");
         }
      else
         if (hide_data) {
            sge_dstring_sprintf_append(&job_output,"%-5s ", "*");
         } else {
            sge_dstring_sprintf_append(&job_output,"%-5.5f ", summary->io_usage);
         }

      /* report jobs dynamic scheduling attributes */
      /* only scheduled have these attribute */
      /* Pending jobs can also have tickets */
      if (summary->is_zombie) {
         sge_dstring_sprintf_append(&job_output,"   NA ");
         sge_dstring_sprintf_append(&job_output,"   NA ");
         sge_dstring_sprintf_append(&job_output,"   NA ");
         sge_dstring_sprintf_append(&job_output,"   NA ");
         sge_dstring_sprintf_append(&job_output,"   NA ");
         sge_dstring_sprintf_append(&job_output,"   NA ");
      } else {
         if (sge_ext || summary->is_queue_assigned) {
            if (hide_data) {
               sge_dstring_sprintf_append(&job_output,"%5s ", "*");
               sge_dstring_sprintf_append(&job_output,"%5s ", "*");
               sge_dstring_sprintf_append(&job_output,"%5s ", "*");
               sge_dstring_sprintf_append(&job_output,"%5s ", "*");
               sge_dstring_sprintf_append(&job_output,"%5s ", "*");
               sge_dstring_sprintf_append(&job_output,"%-5s ", "*");
            } else {
               sge_dstring_sprintf_append(&job_output,"%5d ", (int)summary->tickets),
               sge_dstring_sprintf_append(&job_output,"%5d ", (int)summary->override_tickets);
               sge_dstring_sprintf_append(&job_output,"%5d ", (int)summary->otickets);
               sge_dstring_sprintf_append(&job_output,"%5d ", (int)summary->ftickets);
               sge_dstring_sprintf_append(&job_output,"%5d ", (int)summary->stickets);
               sge_dstring_sprintf_append(&job_output,"%-5.2f ", summary->share);
            }
         } else {
            sge_dstring_sprintf_append(&job_output,"                                          ");
         }
      }
   }
   /* if not full listing we need the queue's name in each line */
   if (!(parameter.full_listing_ & QSTAT_DISPLAY_FULL)) {
      char temp[20];
	   snprintf(temp, sizeof(temp), "%%-%d.%ds ", parameter.longest_queue_length, parameter.longest_queue_length);
      if (hide_data) {
         sge_dstring_sprintf_append(&job_output, temp, summary->queue?"*":"");
      } else {
         sge_dstring_sprintf_append(&job_output, temp, summary->queue?summary->queue:"");
      }
   }

   if ((parameter.group_opt_ & GROUP_NO_PETASK_GROUPS)) {
      /* MASTER/SLAVE information needed only to show parallel job distribution */
      if (summary->master)
         if (hide_data) {
            sge_dstring_sprintf_append(&job_output, "%7s", "*");
         } else {
            sge_dstring_sprintf_append(&job_output,"%-7.6s", summary->master);
         }
      else
         sge_dstring_sprintf_append(&job_output,"       ");
   } else {
      /* job slots requested/granted */
      if (hide_data) {
         sge_dstring_sprintf_append(&job_output, "%5s", "*");
      } else {
         sge_dstring_sprintf_append(&job_output,"%5d ", (int)summary->slots);
      }
   }

   if (summary->task_id && summary->is_array)
      if (hide_data) {
         sge_dstring_sprintf_append(&job_output, "%s", "*");
      } else {
         sge_dstring_sprintf_append(&job_output,"%s", summary->task_id);
      }
   else
      sge_dstring_sprintf_append(&job_output,"       ");

   if (!tsk_ext) {
      sge_dstring_sprintf_append(&job_output,"\n");
   }
   printf("%s", sge_dstring_get_string(&job_output));

   sge_dstring_free(&ds);
   sge_dstring_free(&job_output);

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

   bool indent = false;

   printf("   %s%-12s ", indent ? QSTAT_INDENT2: "", (summary->task_id == nullptr)? "" : summary->task_id );
   printf("%-5.5s ", summary->state);

   if (summary->has_cpu_usage) {
      dstring resource_string = DSTRING_INIT;

      double_print_time_to_dstring(summary->cpu_usage, &resource_string, true);
      printf("%s ", sge_dstring_get_string(&resource_string));
      sge_dstring_free(&resource_string);
   } else {
      printf("%-10.10s ", summary->is_running?"NA":"");
   }
   if (summary->has_mem_usage) {
      printf("%-5.5f ", summary->mem_usage);
   } else {
      printf("%-7.7s ", summary->is_running?"NA":"");
   }

   /* scaled io usage */
   if (summary->has_io_usage) {
      printf("%-5.5f ", summary->io_usage);
   } else {
      printf("%-7.7s ", summary->is_running?"NA":"");
   }

   if (summary->has_exit_status) {
      printf("%-4d", (int)summary->exit_status);
   }

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_sub_tasks_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   putchar('\n');
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_requested_pe(std::ostream &os, const char *pe_name, const char *pe_range) {
   DENTER(TOP_LAYER);
   printf(QSTAT_INDENT QSTAT_R_ATTRIB "%s %s\n", "Requested PE:", pe_name, pe_range);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_granted_pe(std::ostream &os, const char* pe_name, int pe_slots) {
   DENTER(TOP_LAYER);
   printf(QSTAT_INDENT QSTAT_R_ATTRIB "%s %d\n", "Granted PE:", pe_name, pe_slots);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_additional_info(std::ostream &os, job_additional_info_t name, const char *value) {
   DENTER(TOP_LAYER);

   const char *name_str = nullptr;
   switch (name) {
      case CHECKPOINT_ENV: name_str = "Checkpoint Env.:"; break;
      case MASTER_QUEUE:   name_str = "Master Queue:"; break;
      case FULL_JOB_NAME:  name_str = "Full jobname:"; break;
      default:
           DPRINTF("Unknown additional info(%d)\n", name);
           TerminationManager::trigger_abort();
   }
   printf(QSTAT_INDENT QSTAT_R_ATTRIB "%s\n", name_str == nullptr ? "" : name_str, value == nullptr ? "" : value);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_request(std::ostream &os, const char *name, const char *value) {
   DENTER(TOP_LAYER);
   printf(QSTAT_INDENT "%s=%s (default)\n", name, value);
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_hard_requested_queues_started(std::ostream &os, int scope) {
   DENTER(TOP_LAYER);

   switch(scope) {
      case JRS_SCOPE_MASTER:
         printf(QSTAT_INDENT QSTAT_R_ATTRIB, "Master task hard requested queues:");
         break;
      case JRS_SCOPE_SLAVE:
         printf(QSTAT_INDENT QSTAT_R_ATTRIB, "Slave task hard requested queues:");
         break;
      case JRS_SCOPE_GLOBAL:
         printf(QSTAT_INDENT QSTAT_R_ATTRIB , "Hard requested queues:");
      default:
         break;
   }

   hard_requested_queue_count = 0;

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_hard_requested_queue(std::ostream &os, int scope, const char *name) {
   DENTER(TOP_LAYER);

   if (hard_requested_queue_count > 0) {
      printf(", %s", name);
   } else {
      printf("%s", name);
   }
   hard_requested_queue_count++;

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_hard_requested_queues_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   putchar('\n');
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_soft_requested_queues_started(std::ostream &os, int scope) {
   DENTER(TOP_LAYER);

   switch(scope) {
      case JRS_SCOPE_MASTER:
         printf(QSTAT_INDENT QSTAT_R_ATTRIB, "Master task soft requested queues:");
         break;
      case JRS_SCOPE_SLAVE:
         printf(QSTAT_INDENT QSTAT_R_ATTRIB, "Slave task soft requested queues:");
         break;
      case JRS_SCOPE_GLOBAL:
         printf(QSTAT_INDENT QSTAT_R_ATTRIB, "Soft requested queues:");
      default:
         break;
   }

   soft_requested_queue_count = 0;

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_soft_requested_queue(std::ostream &os, int scope, const char *name) {
   DENTER(TOP_LAYER);

   if (soft_requested_queue_count > 0) {
      printf(", %s", name);
   } else {
      printf("%s", name);
   }
   soft_requested_queue_count++;

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_soft_requested_queues_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   putchar('\n');
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_hard_resources_started(std::ostream &os, int scope) {
   DENTER(TOP_LAYER);

   hard_resource_count = 0;

   switch(scope) {
      case JRS_SCOPE_MASTER:
         printf(QSTAT_INDENT QSTAT_R_ATTRIB, "Master Hard Resources:");
         break;
      case JRS_SCOPE_SLAVE:
         printf(QSTAT_INDENT QSTAT_R_ATTRIB, "Slave Hard Resources:");
         break;
      case JRS_SCOPE_GLOBAL:
      default:
         printf(QSTAT_INDENT QSTAT_R_ATTRIB, "Hard Resources:");
         break;
   }

   DRETURN_VOID;
}


void ocs::QStatDefaultViewPlain::report_hard_resource(std::ostream &os, int scope, const char *name, const char *value, double uc) {
   DENTER(TOP_LAYER);
   if (hard_resource_count > 0 ) {
      printf(QSTAT_INDENT QSTAT_R_ATTRIB, " ");
   }
   printf("%s=%s (%f)\n", name, value == nullptr ? "" : value, uc);
   hard_resource_count++;

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_hard_resources_finished(std::ostream &os) {
   DENTER(TOP_LAYER);

   if (hard_resource_count == 0) {
      putchar('\n');
   }

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_soft_resources_started(std::ostream &os, int scope) {
   DENTER(TOP_LAYER);

   soft_resource_count = 0;

   switch(scope) {
      case JRS_SCOPE_MASTER:
         printf(QSTAT_INDENT QSTAT_R_ATTRIB, "Master Soft Resources:");
         break;
      case JRS_SCOPE_SLAVE:
         printf(QSTAT_INDENT QSTAT_R_ATTRIB, "Slave Soft Resources:");
         break;
      case JRS_SCOPE_GLOBAL:
      default:
         printf(QSTAT_INDENT QSTAT_R_ATTRIB, "Soft Resources:");
         break;
   }

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_soft_resource(std::ostream &os, int scope, const char *name, const char *value, double uc) {
   DENTER(TOP_LAYER);

   if (soft_resource_count > 0 ) {
      printf(QSTAT_INDENT QSTAT_R_ATTRIB, " ");
   }
   printf("%s=%s\n", name, value == nullptr ? "" : value);
   soft_resource_count++;

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_soft_resources_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   if (soft_resource_count == 0) {
      putchar('\n');
   }

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_predecessors_requested_started(std::ostream &os) {
   DENTER(TOP_LAYER);

   predecessor_requested_count = 0;
   printf(QSTAT_INDENT QSTAT_R_ATTRIB, "Predecessor Jobs (request):");

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_predecessor_requested(std::ostream &os, const char *name) {
   DENTER(TOP_LAYER);

   if(predecessor_requested_count > 0 ) {
      printf(", %s", name);
   } else {
      printf("%s", name);
   }
   predecessor_requested_count++;

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_predecessors_requested_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   putchar('\n');
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_predecessors_started(std::ostream &os) {
   DENTER(TOP_LAYER);
   predecessor_count = 0;
   printf(QSTAT_INDENT QSTAT_R_ATTRIB, "Predecessor Jobs:");

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_predecessor(std::ostream &os, u_long32 jid) {
   DENTER(TOP_LAYER);
   if (predecessor_count > 0 ) {
      printf(", " sge_u32, jid);
   } else {
      printf(sge_u32, jid);
   }
   predecessor_count++;

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_predecessors_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   putchar('\n');
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_ad_predecessors_requested_started(std::ostream &os) {
   DENTER(TOP_LAYER);

   ad_predecessor_requested_count = 0;
   printf(QSTAT_INDENT QSTAT_R_ATTRIB, "Predecessor Array Jobs (request):");
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_ad_predecessor_requested(std::ostream &os, const char *name) {
   DENTER(TOP_LAYER);

   if (ad_predecessor_requested_count > 0) {
      printf(", %s", name);
   } else {
      printf("%s", name);
   }
   ad_predecessor_requested_count++;

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_ad_predecessors_requested_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   putchar('\n');
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_ad_predecessors_started(std::ostream &os) {
   DENTER(TOP_LAYER);

   ad_predecessor_count = 0;
   printf(QSTAT_INDENT QSTAT_R_ATTRIB, "Predecessor Array Jobs:");

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_ad_predecessor(std::ostream &os, u_long32 jid) {
   DENTER(TOP_LAYER);

   if (ad_predecessor_count > 0) {
      printf(", " sge_u32, jid);
   } else {
      printf(sge_u32, jid);
   }
   ad_predecessor_count++;

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_ad_predecessors_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   putchar('\n');
   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_binding_started(std::ostream &os) {
   DENTER(TOP_LAYER);

   printf(QSTAT_INDENT QSTAT_R_ATTRIB, "Binding:");

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_binding(std::ostream &os, const char *binding) {
   DENTER(TOP_LAYER);

   printf("%s", binding);

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_binding_finished(std::ostream &os) {
   DENTER(TOP_LAYER);

   putchar('\n');

   DRETURN_VOID;
}

void ocs::QStatDefaultViewPlain::report_job_finished(std::ostream &os, u_int jid) {
}
