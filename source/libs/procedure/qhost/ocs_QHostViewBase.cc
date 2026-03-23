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

#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <climits>
#include <cmath>
#include <cfloat>
#include <sstream>
#include <format>

#include "uti/sge_bitfield.h"
#include "uti/sge_hostname.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/ocs_Bootstrap.h"

#include "comm/commlib.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_eval_expression.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_qinstance_type.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_usage.h"

#include "sched/sge_complex_schedd.h"
#include "sched/sge_select_queue.h"
#include "sched/sge_urgency.h"
#include "sched/sge_job_schedd.h"

#include "basis_types.h"
#include "ocs_client_print.h"

#include "qhost/ocs_QHostModel.h"
#include "qhost/ocs_QHostViewXML.h"
#include "qhost/ocs_QHostViewPlain.h"
#include "qhost/ocs_QHostViewBase.h"

ocs::QHostViewBase::QHostViewBase(const QHostParameter &parameter) {
   full_listing_ = parameter.get_show();
}

/*-------------------------------------------------------------------------*/
void
ocs::QHostViewBase::show_host(std::ostream &os, const lListElem *hep, const QHostParameter &parameter, const QHostModel &model, QHostViewBase &report_handler) {
   DENTER(TOP_LAYER);
   lListElem *lep;
   char *s, host_print[CL_MAXHOSTNAMELEN+1] = "";
   char load_avg[20], mem_total[20], mem_used[20], swap_total[20],
        swap_used[20], num_proc[20], socket[20], core[20], arch_string[80], thread[20];
   dstring rs = DSTRING_INIT;
   u_long32 dominant = 0;
   bool ignore_fqdn = ocs::Bootstrap::get_ignore_fqdn();
   const char *host = lGetHost(hep, EH_name);

   /* cut away domain in case of ignore_fqdn */
   sge_strlcpy(host_print, host, CL_MAXHOSTNAMELEN);
   if (ignore_fqdn && (s = strchr(host_print, '.'))) {
      *s = '\0';
   }

   lList *centry_list = model.get_centry_list();

   // arch
   lep = get_attribute_by_name(nullptr, hep, nullptr, LOAD_ATTR_ARCH, centry_list, nullptr, DISPATCH_TIME_NOW, 0);
   if (lep) {
      sge_strlcpy(arch_string, sge_get_dominant_stringval(lep, &dominant, &rs), sizeof(arch_string));
      sge_dstring_clear(&rs);
      lFreeElem(&lep);
   } else {
      strcpy(arch_string, "-");
   }

   // num_proc
   lep= get_attribute_by_name(nullptr, hep, nullptr, LOAD_ATTR_NUM_PROC, centry_list, nullptr, DISPATCH_TIME_NOW, 0);
   if (lep) {
      sge_strlcpy(num_proc, sge_get_dominant_stringval(lep, &dominant, &rs), sizeof(num_proc));
      sge_dstring_clear(&rs);
      lFreeElem(&lep);
   } else {
      strcpy(num_proc, "-");
   }

   // nsoc (sockets)
   lep= get_attribute_by_name(nullptr, hep, nullptr, LOAD_ATTR_SOCKETS, centry_list, nullptr, DISPATCH_TIME_NOW, 0);
   if (lep) {
      sge_strlcpy(socket, sge_get_dominant_stringval(lep, &dominant, &rs), sizeof(socket));
      sge_dstring_clear(&rs);
      lFreeElem(&lep);
   } else {
      strcpy(socket, "-");
   }

   // nthr (threads)
   lep= get_attribute_by_name(nullptr, hep, nullptr, LOAD_ATTR_THREADS, centry_list, nullptr, DISPATCH_TIME_NOW, 0);
   if (lep) {
      sge_strlcpy(thread, sge_get_dominant_stringval(lep, &dominant, &rs), sizeof(thread));
      sge_dstring_clear(&rs);
      lFreeElem(&lep);
   } else {
      strcpy(thread, "-");
   }

   // ncor (cores)
   lep= get_attribute_by_name(nullptr, hep, nullptr, LOAD_ATTR_CORES, centry_list, nullptr, DISPATCH_TIME_NOW, 0);
   if (lep) {
      sge_strlcpy(core, sge_get_dominant_stringval(lep, &dominant, &rs), sizeof(core));
      sge_dstring_clear(&rs);
      lFreeElem(&lep);
   } else {
      strcpy(core, "-");
   }

   // load_avg
   lep= get_attribute_by_name(nullptr, hep, nullptr, "load_avg", centry_list, nullptr, DISPATCH_TIME_NOW, 0);
   if (lep) {
      reformat_double_string(load_avg, sizeof(load_avg), "%.2f%c", sge_get_dominant_stringval(lep, &dominant, &rs));
      sge_dstring_clear(&rs);
      lFreeElem(&lep);
   } else {
      strcpy(load_avg, "-");
   }

   // mem_total
   lep= get_attribute_by_name(nullptr, hep, nullptr, "mem_total", centry_list, nullptr, DISPATCH_TIME_NOW, 0);
   if (lep) {
      reformat_double_string(mem_total, sizeof(mem_total), "%.1f%c", sge_get_dominant_stringval(lep, &dominant, &rs));
      sge_dstring_clear(&rs);
      lFreeElem(&lep);
   } else {
      strcpy(mem_total, "-");
   }

   // mem_used
   lep= get_attribute_by_name(nullptr, hep, nullptr, "mem_used", centry_list, nullptr, DISPATCH_TIME_NOW, 0);
   if (lep) {
      reformat_double_string(mem_used, sizeof(mem_used), "%.1f%c", sge_get_dominant_stringval(lep, &dominant, &rs));
      sge_dstring_clear(&rs);
      lFreeElem(&lep);
   } else {
      strcpy(mem_used, "-");
   }

   // swap_total
   lep= get_attribute_by_name(nullptr, hep, nullptr, "swap_total", centry_list, nullptr, DISPATCH_TIME_NOW, 0);
   if (lep) {
      reformat_double_string(swap_total, sizeof(swap_total), "%.1f%c", sge_get_dominant_stringval(lep, &dominant, &rs));
      sge_dstring_clear(&rs);
      lFreeElem(&lep);
   } else {
      strcpy(swap_total, "-");
   }

   // swap_used
   lep= get_attribute_by_name(nullptr, hep, nullptr, "swap_used", centry_list, nullptr, DISPATCH_TIME_NOW, 0);
   if (lep) {
      reformat_double_string(swap_used, sizeof(swap_used), "%.1f%c", sge_get_dominant_stringval(lep, &dominant, &rs));
      sge_dstring_clear(&rs);
      lFreeElem(&lep);
   } else {
      strcpy(swap_used, "-");
   }


   // hostname
   if (typeid(report_handler) == typeid(ocs::QHostViewPlain)) {
      report_handler.host_value(os, "{:<23} ", nullptr, host ? host_print : "-");
   }

   // values
   report_handler.host_value(os, "{:<13.13} ", "arch_string", arch_string);
   report_handler.host_value(os, "{:>4.4} ", "num_proc", num_proc);
   report_handler.host_value(os, "{:>5.5} ", "m_socket", socket);
   report_handler.host_value(os, "{:>5.5} ", "m_core", core);
   report_handler.host_value(os, "{:>5.5} ", "m_thread", thread);
   report_handler.host_value(os, "{:>6.6} ", "load_avg", load_avg);
   report_handler.host_value(os, "{:>7.7} ", "mem_total", mem_total);
   report_handler.host_value(os, "{:>7.7} ", "mem_used", mem_used);
   report_handler.host_value(os, "{:>7.7} ", "swap_total", swap_total);
   report_handler.host_value(os, "{:>7.7} ", "swap_used", swap_used);

   sge_dstring_free(&rs);

   DRETURN_VOID;
}

/*-------------------------------------------------------------------------*/
void
ocs::QHostViewBase::show_host_queues(std::ostream &os, lListElem *host, QHostParameter &parameter, QHostModel &model, QHostViewBase &report_handler) {
   const lList *load_thresholds, *suspend_thresholds;
   lListElem *qep;
   lListElem *cqueue;
   u_long32 interval;
   const char *ehname = lGetHost(host, EH_name);
   lList *qlp = model.get_queue_list();
   lList *ehl = model.get_exechost_list();
   lList *cl = model.get_centry_list();
   u_long32 show = parameter.get_show();

   DENTER(TOP_LAYER);

   if (!(show & QHOST_DISPLAY_QUEUES) && !(show & QHOST_DISPLAY_JOBS)) {
      DRETURN_VOID;
   }

   for_each_rw(cqueue, qlp) {
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);

      if ((qep=lGetElemHostRW(qinstance_list, QU_qhostname, ehname))) {
         const char *qname = lGetString(qep, QU_qname);

         //if (hide_data) {
         //   continue;
         //}

         if (show & QHOST_DISPLAY_QUEUES) {
            report_handler.queue_start(os, "   {:<20} ", qname);

            /*
            ** qtype
            */
            {
               dstring type_string = DSTRING_INIT;

               // @todo ostringstream
               qinstance_print_qtype_to_dstring(qep, &type_string, true);
               report_handler.queue_value(os, qname, "{:<5.5} ", "qtype_string", sge_dstring_get_string(&type_string));
               sge_dstring_free(&type_string);
            }

            // show reserved/used/tota slots
            report_handler.queue_value(os, qname, "{}/", "slots_resv", qinstance_slots_reserved(qep));
            report_handler.queue_value(os, qname, "{}/", "slots_used", qinstance_slots_used(qep));
            report_handler.queue_value(os, qname, "{} ", "slots", lGetUlong(qep, QU_job_slots));

            /*
            ** state of queue
            */
            load_thresholds = lGetList(qep, QU_load_thresholds);
            suspend_thresholds = lGetList(qep, QU_suspend_thresholds);
            if (sge_load_alarm(nullptr, 0, qep, load_thresholds, ehl, cl, nullptr, true)) {
               qinstance_state_set_alarm(qep, true);
            }
            parse_ulong_val(nullptr, &interval, TYPE_TIM, lGetString(qep, QU_suspend_interval), nullptr, 0);
            if (lGetUlong(qep, QU_nsuspend) != 0 && interval != 0
                && sge_load_alarm(nullptr, 0, qep, suspend_thresholds, ehl, cl, nullptr, false)) {
               qinstance_state_set_suspend_alarm(qep, true);
            }
            {
               dstring state_string = DSTRING_INIT;

               // @todo ostringstream
               qinstance_state_append_to_dstring(qep, &state_string);
               report_handler.queue_value(os, qname, "{}", "state_string", sge_dstring_get_string(&state_string));
               sge_dstring_free(&state_string);
            }

            // Closing tag of CR/LF
            report_handler.queue_end(os);
         }

         /*
         ** tag all jobs, we have only fetched running jobs, so every job
         ** should be visible (necessary for the qstat printing functions)
         */
         if (show & QHOST_DISPLAY_JOBS) {
            u_long32 full_listing = (show & QHOST_DISPLAY_QUEUES) ?
                                    QSTAT_DISPLAY_FULL : 0;
            full_listing = full_listing | QSTAT_DISPLAY_ALL;
            report_handler.show_jobs_per_queue(os, qep, 1, full_listing, "   ", GROUP_NO_PETASK_GROUPS, 10, parameter, model, report_handler);
         }
      }
   }

   DRETURN_VOID;
}


void
ocs::QHostViewBase::show_host_resources(std::ostream &os, lListElem *host, const QHostParameter &parameter, const QHostModel &model, QHostViewBase &report_handler) {
   DENTER(TOP_LAYER);

   lList *rlp = nullptr;
   lListElem *rep;
   char dom[5];
   dstring resource_string = DSTRING_INIT;
   const char *s;
   u_long32 dominant;
   int first = 1;
   lList *ehl = model.get_exechost_list();
   lList *cl = model.get_centry_list();
   const lList *resl = parameter.get_resource_visible_list();
   u_long32 show = parameter.get_show();

   // does the executing qstat user have access to this queue?
   //if (dept_view) {
   //   const char *username = component_get_username();
   //   const char *groupname = component_get_groupname();
   //   int amount;
   //   ocs_grp_elem_t *grp_array;
   //   component_get_supplementray_groups(&amount, &grp_array);
   //   lList *grp_list = grp_list_array2list(amount, grp_array);
   //   hide_data = !sge_has_access_(username, groupname, grp_list, lGetList(host, EH_acl), lGetList(host, EH_xacl), acl_list);
   //   lFreeList(&grp_list);
   //}

   if (!(show & QHOST_DISPLAY_RESOURCES)) {
      DRETURN_VOID;
   }
   host_complexes2scheduler(&rlp, host, ehl, cl);
   for_each_rw(rep, rlp) {
      if (resl != nullptr) {
         lListElem *r1;
         int found = 0;
         for_each_rw (r1, resl) {
            if (!strcmp(lGetString(r1, ST_name), lGetString(rep, CE_name)) ||
                !strcmp(lGetString(r1, ST_name), lGetString(rep, CE_shortcut))) {
               found = 1;
               if (first) {
                  first = 0;
                  // @todo when is this shown
                  if (typeid(report_handler) == typeid(ocs::QHostViewPlain)) {
                     os << std::endl;
                     os << std::format("    Host Resource(s):   ");
                  }
               }
               break;
            }
         }
         if (!found) {
            continue;
         }
      }

      sge_dstring_clear(&resource_string);

      // get the dominant value of the resource for this host
      s = sge_get_dominant_stringval(rep, &dominant, &resource_string);

      // find current usage for m_topology
      std::string details;
      if (strcmp(lGetString(rep, CE_name), LOAD_ATTR_TOPOLOGY) == 0) {
         details = host_get_topology_in_use(host);
      }

      monitor_dominance(dom, dominant);
      report_handler.resource_value(os, dom, lGetString(rep, CE_name), s, details.empty() ? nullptr : details.c_str());
   }
   lFreeList(&rlp);
   sge_dstring_free(&resource_string);

   DRETURN_VOID;
}

void
ocs::QHostViewBase::reformat_double_string(char *new_string, const size_t result_size, const char *format, const char *old_string)
{
   DENTER(TOP_LAYER);

   double dval;
   if (parse_ulong_val(&dval, nullptr, TYPE_MEM, old_string, nullptr, 0)) {
      if (dval == DBL_MAX) {
         std::strcpy(new_string, "infinity");
      } else {
         static const char units[] = { '\0', 'K', 'M', 'G', 'T', 'P', 'E' };

         double absval = std::fabs(dval);
         int unit_index = 0;

         while (absval >= 1024.0 && unit_index < 6) {
            dval /= 1024.0;
            absval /= 1024.0;
            ++unit_index;
         }

         std::snprintf(new_string, result_size, format, dval, units[unit_index]);
      }
   } else {
      std::strcpy(new_string, "?");
   }

   DRETURN_VOID;
}


// slots_per_line: number of slots to be printed in slots column when 0 is passed the number of requested slots printed
void
ocs::QHostViewBase::show_job(std::ostream &os, lListElem *job, lListElem *jatep, lListElem *qep, int print_jobid, const char *master,
                                  dstring *dyn_task_str, u_long32 full_listing, int slots, int slot,
                                  const char *indent, u_long32 group_opt, int slots_per_line,
                                  int queue_name_length, QHostParameter &parameter, QHostModel &model, QHostViewBase &report_handler) {
   DENTER(TOP_LAYER);
   char state_string[8];
   u_long32 jstate;
   int sge_urg, sge_pri, sge_ext, sge_time;
   const lListElem *gdil_ep = nullptr;
   int running;
   const char *queue_name = nullptr;
   int tsk_ext;
   u_long tickets, otickets, stickets, ftickets;
   dstring ds;
   char buffer[128];
   dstring queue_name_buffer = DSTRING_INIT;
   u_long32 jid = lGetUlong(job, JB_job_number);
   lList *pe_list = model.get_pe_list();

   sge_dstring_init(&ds, buffer, sizeof(buffer));

   if (qep != nullptr) {
      queue_name = qinstance_get_name(qep, &queue_name_buffer);
   }

   sge_ext = ((full_listing & QSTAT_DISPLAY_EXTENDED) == QSTAT_DISPLAY_EXTENDED);
   tsk_ext = (full_listing & QSTAT_DISPLAY_TASKS);
   sge_urg = (full_listing & QSTAT_DISPLAY_URGENCY);
   sge_pri = (full_listing & QSTAT_DISPLAY_PRIORITY);
   sge_time = !sge_ext;
   sge_time = sge_time | tsk_ext | sge_urg | sge_pri;

   // print header

   std::ostringstream oss;
   static int first_time = 1;
   if (first_time) {
      if (!(full_listing & QSTAT_DISPLAY_FULL)) {
         first_time = 0;

         oss << indent;
         oss << std::format("{:<10} ", "job-ID");
         oss << std::format("{:<7} ", "prior");
         if (sge_pri || sge_urg) {
            oss << std::format("{:<8} ", "nurg");
         }
         if (sge_pri) {
            oss << std::format("{:<8} ", "npprior");
         }
         if (sge_pri || sge_ext) {
            oss << std::format("{:<8} ", "ntckts");
         }
         if (sge_urg) {
            oss << std::format("{:<9} ", "urg");
            oss << std::format("{:<9} ", "rrcontr");
            oss << std::format("{:<9} ", "wtcontr");
            oss << std::format("{:<9} ", "dlcontr");
         }
         if (sge_pri) {
            oss << std::format("{:<6} ", "ppri");
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
            oss << std::format("{:<10} ", USAGE_ATTR_CPU);
            oss << std::format("{:<10} ", USAGE_ATTR_MEM);
            oss << std::format("{:<10} ", USAGE_ATTR_IO);
            oss << std::format("{:<6} ", "tckts");
            oss << std::format("{:<6} ", "ovrts");
            oss << std::format("{:<6} ", "otckt");
            oss << std::format("{:<6} ", "ftckt");
            oss << std::format("{:<6} ", "stckt");
            oss << std::format("{:<6} ", "share");
         }

         // dynamic queue length
         std::string fmt = std::format("{{:<{}.{}}} ", queue_name_length, queue_name_length);
         oss << std::vformat(fmt, std::make_format_args("queue"));

         oss << std::format("{:<6} ", (group_opt & GROUP_NO_PETASK_GROUPS) ? "master" : "slots");
         oss << std::format("{:<10} ", "ja-task-ID");

         if (tsk_ext) {
            oss << std::format("{:<10} ", "task-ID");
            oss << std::format("{:<6} ", "state");
            oss << std::format("{:<10} ", USAGE_ATTR_CPU);
            oss << std::format("{:<10} ", USAGE_ATTR_MEM);
            oss << std::format("{:<10} ", USAGE_ATTR_IO);
            oss << std::format("{:<6} ", "stat");
            oss << std::format("{:<7} ", "failed");
         }
         oss << std::endl;

         // print seperator line
         size_t length_header = oss.str().length() - strlen(indent) - 1;
         oss << indent << std::string(length_header, '-') << std::endl;

      }
   }
   report_handler.job_start(os, oss.str().c_str(), jid);

   // Only Plain: indent
   report_handler.job_value(os, jid, indent, nullptr, nullptr);

   // Only Plain: job number
   // @todo avoid the cast - use template method
   report_handler.job_value(os, jid, "{:>10} ", nullptr, (u_long64)jid);

   /* per job priority information */
   {
      if (typeid(report_handler) == typeid(ocs::QHostViewXML) || print_jobid) {
         report_handler.job_value(os, jid, "{:<7.5f} ", "priority", lGetDouble(jatep, JAT_prio));
      } else {
         report_handler.job_value(os, jid, std::string(7 + 1, ' ').c_str(), nullptr, nullptr);
      }

      if (sge_pri || sge_urg) {
         if (print_jobid) {
            report_handler.job_value(os, jid, "{:<7.5f} ", "nurg", lGetDouble(job, JB_nurg));
         } else {
            report_handler.job_value(os, jid, std::string(7 + 1, ' ').c_str(), nullptr, nullptr);
         }
      }

      if (sge_pri) {
         if (print_jobid) {
            report_handler.job_value(os, jid, "{:<7.5f} ", "nppri", lGetDouble(job, JB_nppri));
         } else {
            report_handler.job_value(os, jid, std::string(7 + 1, ' ').c_str(), nullptr, nullptr);
         }
      }

      if (sge_pri || sge_ext) {
         if (print_jobid) {
            report_handler.job_value(os, jid, "{:<7.5f} ", "ntix", lGetDouble(job, JAT_ntix));
         } else {
            report_handler.job_value(os, jid, std::string(7 + 1, ' ').c_str(), nullptr, nullptr);
         }
      }

      if (sge_urg) {
         if (print_jobid) {
            constexpr double max = 99999999;

            double value = lGetDouble(job, JB_urg);
            report_handler.job_value(os, jid, value > max ? "{:<8.3g} " : "{:<8.0f ", "urg", value);
            value = lGetDouble(job, JB_rrcontr);
            report_handler.job_value(os, jid, value > max ? "{:<8.3g} " : "{:<8.0f ", "rrcontr", value);
            value = lGetDouble(job, JB_wtcontr);
            report_handler.job_value(os, jid, value > max ? "{:<8.3g} " : "{:<8.0f ", "wtcontr", value);
            value = lGetDouble(job, JB_dlcontr);
            report_handler.job_value(os, jid, value > max ? "{:<8.3g} " : "{:<8.0f ", "dlcontr", value);
         } else {
            report_handler.job_value(os, jid, std::string(4 * (8 + 1), ' ').c_str(), nullptr, nullptr);
         }
      }

      if (sge_pri) {
         if (print_jobid) {
            // @todo avoid the cast - use template method
            report_handler.job_value(os, jid, "{:<5d} ", "priority", (u_long64)lGetUlong(job, JB_priority) - BASE_PRIORITY);
         } else {
            // @todo we had a gap for 2*6 character before - why?
            report_handler.job_value(os, jid, std::string(5 + 1, ' ').c_str(), nullptr, nullptr);
         }
      }
   }

   // XML: show qinstance name in any case
   if (typeid(report_handler) == typeid(ocs::QHostViewXML)) {
      report_handler.job_value(os, jid, nullptr, "qinstance_name", queue_name);
   }

   // XML and Plain with jobid: show job name, owner
   // or in Plain without jobid: just fill the gap
   if (typeid(report_handler) == typeid(ocs::QHostViewXML) || print_jobid) {
      report_handler.job_value(os, jid, "{:<10.10} ", "job_name", lGetString(job, JB_job_name));
      report_handler.job_value(os, jid, "{:<12.12} ", "job_owner", lGetString(job, JB_owner));
   } else {
      report_handler.job_value(os, jid, std::string(10 + 1, ' ').c_str(), nullptr, nullptr);
      report_handler.job_value(os, jid, std::string(12 + 1, ' ').c_str(), nullptr, nullptr);
   }

   if (sge_ext) {
      if (print_jobid) {
         const char *value = lGetString(job, JB_project);
         report_handler.job_value(os, jid, "{:<16.16} ", "project", value != nullptr ? value : "NA");
         value = lGetString(job, JB_department);
         report_handler.job_value(os, jid, "{:<10.10} ", "department", value != nullptr? value : "NA");
      } else {
         report_handler.job_value(os, jid, std::string(16 + 1, ' ').c_str(), nullptr, nullptr);
         report_handler.job_value(os, jid, std::string(10 + 1, ' ').c_str(), nullptr, nullptr);
      }
   }

   /* move status info into state info */
   jstate = lGetUlong(jatep, JAT_state);
   if (lGetUlong(jatep, JAT_status) == JTRANSFERING) {
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

   // XML and Plain with jobid: show job_state
   // or in Plain without jobid: just fill the gap
   if (typeid(report_handler) == typeid(ocs::QHostViewXML) || print_jobid) {
      job_get_state_string(state_string, jstate);
      report_handler.job_value(os, jid, "{:<5.5} ", "job_state", state_string);
   } else {
      report_handler.job_value(os, jid, std::string(6, ' ').c_str(), nullptr, nullptr);
   }

   if (sge_time) {
      if (print_jobid) {
         const char *name;
         u_long64 value;
         if (u_long64 jat_start_time = lGetUlong64(jatep, JAT_start_time); jat_start_time != 0) {
            name = "start_time";
            value = jat_start_time;
         } else {
            name = "submit_time";
            value = lGetUlong64(job, JB_submission_time);
         }
         if (typeid(report_handler) == typeid(QHostViewPlain)) {
            const char *time_string = sge_ctime64_short(value, &ds);
            report_handler.job_value(os, jid, "{} ", name, time_string);
         } else {
            report_handler.job_value(os, jid, "{} ", name, value);
         }
      } else {
         report_handler.job_value(os, jid, std::string(20, ' ').c_str(), nullptr, nullptr);
      }
   }

   /* is job logically running */
   running = lGetUlong(jatep, JAT_status) == JRUNNING || lGetUlong(jatep, JAT_status) == JTRANSFERING;

   /* deadline time */
   if (sge_urg) {
      u_long64 deadline = lGetUlong64(job, JB_deadline);
      if (print_jobid && deadline != 0) {
         report_handler.job_value(os, jid, "{} ", "deadline", sge_ctime64_short(deadline, &ds));
      } else {
         report_handler.job_value(os, jid, std::string(20, ' ').c_str(), nullptr, nullptr);
      }
   }

   if (sge_ext) {
      const lListElem *up, *pe, *task;
      lList *job_usage_list;
      const char *pe_name;

      if (!master || !strcmp(master, "MASTER")) {
         job_usage_list = lCopyList(nullptr, lGetList(jatep, JAT_scaled_usage_list));
      } else {
         job_usage_list = lCreateList("", UA_Type);
      }

      /* sum pe-task usage based on queue slots */
      if (job_usage_list) {
         int subtask_ndx = 1;
         for_each_ep(task, lGetList(jatep, JAT_task_list)) {
            const lListElem *src, *ep;
            lListElem *dst;
            const char *qname;

            if (!slots || (queue_name && ((ep = lFirst(lGetList(task, PET_granted_destin_identifier_list)))) &&
                           ((qname = lGetString(ep, JG_qname))) && !strcmp(qname, queue_name) &&
                           ((subtask_ndx++ % slots) == slot))) {
               for_each_ep(src, lGetList(task, PET_scaled_usage)) {
                  if ((dst = lGetElemStrRW(job_usage_list, UA_name, lGetString(src, UA_name)))) {
                     lSetDouble(dst, UA_value, lGetDouble(dst, UA_value) + lGetDouble(src, UA_value));
                  } else {
                     lAppendElem(job_usage_list, lCopyElem(src));
                  }
               }
            }
         }
      }

      /* scaled cpu usage */
      up = lGetElemStr(job_usage_list, UA_name, USAGE_ATTR_CPU);
      if (up != nullptr) {
         int secs = lGetDouble(up, UA_value);
         int days = secs / (60 * 60 * 24);
         secs -= days * (60 * 60 * 24);
         int hours = secs / (60 * 60);
         secs -= hours * (60 * 60);
         int minutes = secs / 60;
         secs -= minutes * 60;

         std::ostringstream oss;
         oss << std::format("{}:{:02}:{:02}:{:02} ", days, hours, minutes, secs);
         report_handler.job_value(os, jid, oss.str().c_str(), nullptr, nullptr);
      } else {
         report_handler.job_value(os, jid, "{:<10.10}", nullptr, running ? "NA" : "");
      }
      /* scaled mem usage */
      up = lGetElemStr(job_usage_list, UA_name, USAGE_ATTR_MEM);
      if (up != nullptr) {
         report_handler.job_value(os, jid, "{:<5.5f}", nullptr, lGetDouble(up, UA_value));
      } else {
         report_handler.job_value(os, jid, "{:<7.7}", nullptr, running ? "NA" : "");
      }

      /* scaled io usage */
      up = lGetElemStr(job_usage_list, UA_name, USAGE_ATTR_IO);
      if (up != nullptr) {
         report_handler.job_value(os, jid, "{:<5.5f}", nullptr, lGetDouble(up, UA_value));
      } else {
         report_handler.job_value(os, jid, "{:<7.7}", nullptr, running ? "NA" : "");
      }

      lFreeList(&job_usage_list);

      /* get tickets for job/slot */
      /* braces needed to suppress compiler warnings */
      if ((pe_name = lGetString(jatep, JAT_granted_pe)) && (pe = pe_list_locate(pe_list, pe_name)) &&
          lGetBool(pe, PE_control_slaves) && slots &&
          (gdil_ep = lGetSubStr(jatep, JG_qname, queue_name, JAT_granted_destin_identifier_list))) {
         if (slot == 0) {
            tickets = (u_long)lGetDouble(gdil_ep, JG_ticket);
            otickets = (u_long)lGetDouble(gdil_ep, JG_oticket);
            ftickets = (u_long)lGetDouble(gdil_ep, JG_fticket);
            stickets = (u_long)lGetDouble(gdil_ep, JG_sticket);
         } else {
            if (slots) {
               tickets = (u_long)(lGetDouble(gdil_ep, JG_ticket) / slots);
               otickets = (u_long)(lGetDouble(gdil_ep, JG_oticket) / slots);
               ftickets = (u_long)(lGetDouble(gdil_ep, JG_fticket) / slots);
               stickets = (u_long)(lGetDouble(gdil_ep, JG_sticket) / slots);
            } else {
               tickets = otickets = ftickets = stickets = 0;
            }
         }
      } else {
         tickets = (u_long)lGetDouble(jatep, JAT_tix);
         otickets = (u_long)lGetDouble(jatep, JAT_oticket);
         ftickets = (u_long)lGetDouble(jatep, JAT_fticket);
         stickets = (u_long)lGetDouble(jatep, JAT_sticket);
      }

      /* report jobs dynamic scheduling attributes */
      /* only scheduled have these attribute */
      /* Pending jobs can also have tickets */
      if (sge_ext || lGetList(jatep, JAT_granted_destin_identifier_list)) {
         report_handler.job_value(os, jid, "{:<5.5d} ", nullptr, tickets);
         report_handler.job_value(os, jid, "{:<5.5d} ", nullptr, (u_long64)lGetUlong(job, JB_override_tickets));
         report_handler.job_value(os, jid, "{:<5.5d} ", nullptr, otickets);
         report_handler.job_value(os, jid, "{:<5.5d} ", nullptr, ftickets);
         report_handler.job_value(os, jid, "{:<5.5d} ", nullptr, stickets);
         report_handler.job_value(os, jid, "{:<5.2f} ", nullptr, lGetDouble(jatep, JAT_share));
      } else {
         report_handler.job_value(os, jid, "{:5s} ", nullptr, "NA");
         report_handler.job_value(os, jid, "{:5s} ", nullptr, "NA");
         report_handler.job_value(os, jid, "{:5s} ", nullptr, "NA");
         report_handler.job_value(os, jid, "{:5s} ", nullptr, "NA");
         report_handler.job_value(os, jid, "{:5s} ", nullptr, "NA");
         report_handler.job_value(os, jid, "{:5s} ", nullptr, "NA");
      }
   }

   /* if not full listing we need the queue's name in each line */
   if (!(full_listing & QSTAT_DISPLAY_FULL)) {
      std::string fmt = std::format("{{:<{}.{}}} ", queue_name_length, queue_name_length);
      report_handler.job_value(os, jid, fmt.c_str(), "queue_name", queue_name);
   }

   if ((group_opt & GROUP_NO_PETASK_GROUPS)) {
      /* MASTER/SLAVE information needed only to show parallel job distribution */
      if (typeid(report_handler) == typeid(ocs::QHostViewXML) || master) {
         report_handler.job_value(os, jid, "{:<6.6s} ","pe_master", master);
      } else {
         report_handler.job_value(os, jid, "{:<6.6s} ", "pe_master", std::string(7, ' ').c_str());
      }
   } else {
      /* job slots requested/granted */
      if (!slots_per_line) {
         slots_per_line = sge_job_slot_request(job, pe_list);
      }
      // @todo avoid the cast - use template method
      report_handler.job_value(os, jid, "{:<6.6d} ","slots_per_line", (u_long64)slots_per_line);
   }

   // ja-task-ID
   // If we have an ID for the ja-task, we print it.
   // If not, we print an empty string in the plain output, but nothing in the XML output
   if (const char *taskid = sge_dstring_get_string(dyn_task_str); taskid != nullptr && job_is_array(job)) {
      report_handler.job_value(os, jid, "{:<10.10s} ", "taskid", taskid);
   } else if (typeid(report_handler) == typeid(ocs::QHostViewPlain)) {
      report_handler.job_value(os, jid, "{:<10.10s} ", "taskid", std::string(10 + 1, ' ').c_str());
   }

   report_handler.job_end(os);
   sge_dstring_free(&queue_name_buffer);

   DRETURN_VOID;
}

/*-------------------------------------------------------------------------*/
/* print jobs per queue                                                    */
/*-------------------------------------------------------------------------*/
void
ocs::QHostViewBase::show_jobs_per_queue(std::ostream &os, lListElem *qep, int print_jobs_of_queue, u_long32 full_listing,
                                        const char *indent, u_long32 group_opt, int queue_name_length,
                                        QHostParameter &parameter, QHostModel &model, QHostViewBase &report_handler) {
   lListElem *jlep;
   lListElem *jatep;
   const lListElem *gdilep;
   u_long32 job_tag;
   u_long32 jid = 0, old_jid;
   u_long32 jataskid = 0, old_jataskid;
   const char *qnm;
   dstring dyn_task_str = DSTRING_INIT;

   DENTER(TOP_LAYER);

   qnm = lGetString(qep, QU_full_name);

   lList *job_list = model.get_job_list();
   lList *pe_list = model.get_pe_list();

   // @todo why the dependency to the user list? Is there some code missing for qhost?
   lList *user_list = nullptr;
   for_each_rw(jlep, job_list) {
      int master, i;

      for_each_rw(jatep, lGetList(jlep, JB_ja_tasks)) {
         u_long32 jstate = lGetUlong(jatep, JAT_state);

         //if (shut_me_down) {
         //   DRETURN(1);
         //}

         if (ISSET(jstate, JSUSPENDED_ON_SUBORDINATE) || ISSET(jstate, JSUSPENDED_ON_SLOTWISE_SUBORDINATE)) {
            lSetUlong(jatep, JAT_state, jstate & ~JRUNNING);
         }

         gdilep = lGetElemStr(lGetList(jatep, JAT_granted_destin_identifier_list), JG_qname, qnm);
         if (gdilep != nullptr) {
            int slot_adjust = 0;
            int lines_to_print;
            int slots_per_line, slots_in_queue = lGetUlong(gdilep, JG_slots);

            job_tag = lGetUlong(jatep, JAT_suitable);
            job_tag |= TAG_FOUND_IT;
            lSetUlong(jatep, JAT_suitable, job_tag);

            master = !strcmp(qnm, lGetString(lFirst(lGetList(jatep, JAT_granted_destin_identifier_list)), JG_qname));

            if (master) {
               const char *pe_name;
               lListElem *pe;
               if (((pe_name = lGetString(jatep, JAT_granted_pe))) && ((pe = pe_list_locate(pe_list, pe_name))) &&
                   !lGetBool(pe, PE_job_is_first_task)) {
                  slot_adjust = 1;
               }
            }

            /* job distribution view ? */
            if (!(group_opt & GROUP_NO_PETASK_GROUPS)) {
               /* no - condensed ouput format */
               if (!master && !(full_listing & QSTAT_DISPLAY_FULL)) {
                  /* skip all slave outputs except in full display mode */
                  continue;
               }

               /* print only on line per job for this queue */
               lines_to_print = 1;

               /* always only show the number of job slots represented by the line */
               if ((full_listing & QSTAT_DISPLAY_FULL)) {
                  slots_per_line = slots_in_queue;
               } else {
                  slots_per_line = sge_granted_slots(lGetList(jatep, JAT_granted_destin_identifier_list));
               }
            } else {
               /* yes */
               lines_to_print = (int)slots_in_queue + slot_adjust;
               slots_per_line = 1;
            }

            for (i = 0; i < lines_to_print; i++) {
               int already_printed = 0;

               if (!lGetNumberOfElem(user_list) ||
                   (lGetNumberOfElem(user_list) && (lGetUlong(jatep, JAT_suitable) & TAG_SELECT_IT))) {
                  if (print_jobs_of_queue && (job_tag & TAG_SHOW_IT)) {
                     int different, print_jobid;

                     old_jid = jid;
                     jid = lGetUlong(jlep, JB_job_number);
                     old_jataskid = jataskid;
                     jataskid = lGetUlong(jatep, JAT_task_number);
                     sge_dstring_sprintf(&dyn_task_str, sge_u32, jataskid);
                     different = (jid != old_jid) || (jataskid != old_jataskid);

                     if (different) {
                        print_jobid = 1;
                     } else {
                        if (!(full_listing & QSTAT_DISPLAY_RUNNING)) {
                           print_jobid = master && (i == 0);
                        } else {
                           print_jobid = 0;
                        }
                     }

                     if (!already_printed && (full_listing & QSTAT_DISPLAY_RUNNING) &&
                         (lGetUlong(jatep, JAT_state) & JRUNNING)) {
                        report_handler.show_job(os, jlep, jatep, qep, print_jobid,
                                      (master && different && (i == 0)) ? "MASTER" : "SLAVE", &dyn_task_str,
                                      full_listing, slots_in_queue + slot_adjust, i, indent,
                                      group_opt, slots_per_line, queue_name_length, parameter, model, report_handler);
                        already_printed = 1;
                     }
                     if (!already_printed && (full_listing & QSTAT_DISPLAY_SUSPENDED) &&
                         ((lGetUlong(jatep, JAT_state) & JSUSPENDED) ||
                          (lGetUlong(jatep, JAT_state) & JSUSPENDED_ON_THRESHOLD) ||
                          (lGetUlong(jatep, JAT_state) & JSUSPENDED_ON_SUBORDINATE) ||
                          (lGetUlong(jatep, JAT_state) & JSUSPENDED_ON_SLOTWISE_SUBORDINATE))) {
                        report_handler.show_job(os, jlep, jatep, qep, print_jobid,
                                      (master && different && (i == 0)) ? "MASTER" : "SLAVE", &dyn_task_str,
                                      full_listing, slots_in_queue + slot_adjust, i, indent,
                                      group_opt, slots_per_line, queue_name_length, parameter, model, report_handler);
                        already_printed = 1;
                     }

                     if (!already_printed && (full_listing & QSTAT_DISPLAY_USERHOLD) &&
                         (lGetUlong(jatep, JAT_hold) & MINUS_H_TGT_USER)) {
                        report_handler.show_job(os, jlep, jatep, qep, print_jobid,
                                      (master && different && (i == 0)) ? "MASTER" : "SLAVE", &dyn_task_str,
                                      full_listing, slots_in_queue + slot_adjust, i, indent,
                                      group_opt, slots_per_line, queue_name_length, parameter, model, report_handler);
                        already_printed = 1;
                     }

                     if (!already_printed && (full_listing & QSTAT_DISPLAY_OPERATORHOLD) &&
                         (lGetUlong(jatep, JAT_hold) & MINUS_H_TGT_OPERATOR)) {
                        report_handler.show_job(os, jlep, jatep, qep, print_jobid,
                                      (master && different && (i == 0)) ? "MASTER" : "SLAVE", &dyn_task_str,
                                      full_listing, slots_in_queue + slot_adjust, i, indent,
                                      group_opt, slots_per_line, queue_name_length, parameter, model, report_handler);
                        already_printed = 1;
                     }

                     if (!already_printed && (full_listing & QSTAT_DISPLAY_SYSTEMHOLD) &&
                         (lGetUlong(jatep, JAT_hold) & MINUS_H_TGT_SYSTEM)) {
                        report_handler.show_job(os, jlep, jatep, qep, print_jobid,
                                      (master && different && (i == 0)) ? "MASTER" : "SLAVE", &dyn_task_str,
                                      full_listing, slots_in_queue + slot_adjust, i, indent,
                                      group_opt, slots_per_line, queue_name_length, parameter, model, report_handler);
                        already_printed = 1;
                     }

                     if (!already_printed && (full_listing & QSTAT_DISPLAY_JOBARRAYHOLD) &&
                         (lGetUlong(jatep, JAT_hold) & MINUS_H_TGT_JA_AD)) {
                        report_handler.show_job(os, jlep, jatep, qep, print_jobid,
                                      (master && different && (i == 0)) ? "MASTER" : "SLAVE", &dyn_task_str,
                                      full_listing, slots_in_queue + slot_adjust, i, indent, group_opt,
                                      slots_per_line, queue_name_length, parameter, model, report_handler);
                        already_printed = 1;
                     }
                  }
               }
            }
         }
      }
   }
   sge_dstring_free(&dyn_task_str);

   DRETURN_VOID;
}


