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

/** @file
 * @brief Base view of `qhost`, and the interface the three output formats implement
 */

#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <climits>
#include <cmath>
#include <cfloat>
#include <sstream>
#include <format>

#include "uti/sge.h"               // INFINITY_STR
#include "uti/sge_bitfield.h"
#include "uti/sge_hostname.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/ocs_Bootstrap.h"
#include "uti/ocs_FormatText.h"

#include "comm/commlib.h"

#include "sgeobj/cull/sge_all_listsL.h"
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
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_JRE.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_UA.h"

#include "sched/sge_complex_schedd.h"
#include "sched/sge_select_queue.h"
#include "sched/sge_urgency.h"
#include "sched/sge_job_schedd.h"

#include "qhost/ocs_QHostModelBase.h"
#include "qhost/ocs_QHostViewXML.h"
#include "qhost/ocs_QHostViewPlain.h"
#include "qhost/ocs_QHostViewBase.h"

#include "qstat/ocs_QStatParameter.h"

#include "ocs_QHostViewJSON.h"

/** @brief Build a view for one qhost call
 * @param parameter the call's parameters; the view keeps what it needs for formatting
 */
ocs::QHostViewBase::QHostViewBase(const QHostParameter &parameter) : ProcedureView(parameter) {
   full_listing_ = parameter.get_show();
}

/** @brief Render one execution host and everything under it
 * @param os stream to write to
 * @param hep the execution host
 * @param parameter the call's parameters
 * @param model the fetched lists
 * @param report_handler the view whose hooks are called; normally `*this`
 * @return none
 */
void
ocs::QHostViewBase::show_host(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::EH> hep,
                              const QHostParameter &parameter, const QHostModelBase &model,
                              QHostViewBase &report_handler) {
   DENTER(TOP_LAYER);
   char *s, host_print[CL_MAXHOSTNAMELEN+1] = "";
   dstring rs = DSTRING_INIT;
   uint32_t dominant = 0;
   bool ignore_fqdn = ocs::Bootstrap::get_ignore_fqdn();
   const char *host = hep.get<EH_name>();

   /* cut away domain in case of ignore_fqdn */
   sge_strlcpy(host_print, host, CL_MAXHOSTNAMELEN);
   if (ignore_fqdn && (s = strchr(host_print, '.'))) {
      *s = '\0';
   }

   const auto centry_list = model.get_centry_list();
   const auto load_adjustments = model.get_job_load_adjustments();

   // the value of one attribute of this host; neither the global host nor a queue take part
   const auto host_attribute = [&](const char *name) {
      return get_attribute_by_name(ocs::tom::ConstElement<ocs::tom::EH>(), hep, ocs::tom::ConstElement<ocs::tom::QU>(),
                                   name, centry_list, load_adjustments, DISPATCH_TIME_NOW, 0);
   };

   // arch
   char arch_string[80];
   if (const auto lep = host_attribute(LOAD_ATTR_ARCH); lep != nullptr) {
      sge_strlcpy(arch_string, sge_get_dominant_stringval(*lep, &dominant, &rs, nullptr, nullptr),
                  sizeof(arch_string));
      sge_dstring_clear(&rs);
   } else {
      strcpy(arch_string, "-");
   }

   // num_proc
   char num_proc[20];
   uint64_t num_proc_value = 0;
   if (const auto lep = host_attribute(LOAD_ATTR_NUM_PROC); lep != nullptr) {
      const char *np_str = sge_get_dominant_stringval(*lep, &dominant, &rs, nullptr, &num_proc_value);
      sge_strlcpy(num_proc, np_str, sizeof(num_proc));
      sge_dstring_clear(&rs);
   } else {
      strcpy(num_proc, "-");
   }

   // nsoc (sockets)
   char socket[20];
   uint64_t sockets_value = 0;
   if (const auto lep = host_attribute(LOAD_ATTR_SOCKETS); lep != nullptr) {
      const char *socket_str = sge_get_dominant_stringval(*lep, &dominant, &rs, nullptr, &sockets_value);
      sge_strlcpy(socket, socket_str, sizeof(socket));
      sge_dstring_clear(&rs);
   } else {
      strcpy(socket, "-");
   }

   // nthr (threads)
   char thread[20];
   uint64_t threads_value = 0;
   if (const auto lep = host_attribute(LOAD_ATTR_THREADS); lep != nullptr) {
      const char *threads_str = sge_get_dominant_stringval(*lep, &dominant, &rs, nullptr, &threads_value);
      sge_strlcpy(thread, threads_str, sizeof(thread));
      sge_dstring_clear(&rs);
   } else {
      strcpy(thread, "-");
   }

   // ncor (cores)
   char core[20];
   uint64_t cores_value = 0;
   if (const auto lep = host_attribute(LOAD_ATTR_CORES); lep != nullptr) {
      const char *cores_str = sge_get_dominant_stringval(*lep, &dominant, &rs, nullptr, &cores_value);
      sge_strlcpy(core, cores_str, sizeof(core));
      sge_dstring_clear(&rs);
   } else {
      strcpy(core, "-");
   }

   // load_avg
   char load_avg[20];
   double load_avg_value = 0.0;
   if (const auto lep = host_attribute("load_avg"); lep != nullptr) {
      const char *load_str = sge_get_dominant_stringval(*lep, &dominant, &rs, &load_avg_value, nullptr);
      reformat_double_string(load_avg, sizeof(load_avg), "%.2f%c", load_str);
      sge_dstring_clear(&rs);
   } else {
      strcpy(load_avg, "-");
   }

   // mem_total
   char mem_total[20];
   uint64_t mem_total_value = 0;
   if (const auto lep = host_attribute("mem_total"); lep != nullptr) {
      const char *mem_total_str = sge_get_dominant_stringval(*lep, &dominant, &rs, nullptr, &mem_total_value);
      reformat_double_string(mem_total, sizeof(mem_total), "%.1f%c", mem_total_str);
      sge_dstring_clear(&rs);
   } else {
      strcpy(mem_total, "-");
   }

   // mem_used
   char mem_used[20];
   uint64_t mem_used_value = 0;
   if (const auto lep = host_attribute("mem_used"); lep != nullptr) {
      const char *mem_used_str = sge_get_dominant_stringval(*lep, &dominant, &rs, nullptr, &mem_used_value);
      reformat_double_string(mem_used, sizeof(mem_used), "%.1f%c", mem_used_str);
      sge_dstring_clear(&rs);
   } else {
      strcpy(mem_used, "-");
   }

   // swap_total
   char swap_total[20];
   uint64_t swap_total_value = 0;
   if (const auto lep = host_attribute("swap_total"); lep != nullptr) {
      const char *swap_total_str = sge_get_dominant_stringval(*lep, &dominant, &rs, nullptr, &swap_total_value);
      reformat_double_string(swap_total, sizeof(swap_total), "%.1f%c", swap_total_str);
      sge_dstring_clear(&rs);
   } else {
      strcpy(swap_total, "-");
   }

   // swap_used
   char swap_used[20];
   uint64_t swap_used_value = 0;
   if (const auto lep = host_attribute("swap_used"); lep != nullptr) {
      const char *swap_used_str = sge_get_dominant_stringval(*lep, &dominant, &rs, nullptr, &swap_used_value);
      reformat_double_string(swap_used, sizeof(swap_used), "%.1f%c", swap_used_str);
      sge_dstring_clear(&rs);
   } else {
      strcpy(swap_used, "-");
   }


   // hostname
   if (typeid(report_handler) == typeid(QHostViewPlain)) {
      report_handler.host_value(os, "{:<23} ", nullptr, host ? host_print : "-");
   }

   // values
   report_handler.host_value(os, "{:<13.13} ", "arch_string", arch_string);
   if (typeid(report_handler) == typeid(QHostViewJSON)) {
      report_handler.host_value(os, "{:>4.4} ", "num_proc", num_proc_value);
      report_handler.host_value(os, "{:>5.5} ", "m_socket", sockets_value);
      report_handler.host_value(os, "{:>5.5} ", "m_core", cores_value);
      report_handler.host_value(os, "{:>5.5} ", "m_thread", threads_value);
      report_handler.host_value(os, "{:>6.6} ", "load_avg", load_avg_value);
      report_handler.host_value(os, "{:>7.7} ", "mem_total", mem_total_value);
      report_handler.host_value(os, "{:>7.7} ", "mem_used", mem_used_value);
      report_handler.host_value(os, "{:>7.7} ", "swap_total", swap_total_value);
      report_handler.host_value(os, "{:>7.7} ", "swap_used", swap_used_value);
   } else {
      // Plain and XML
      report_handler.host_value(os, "{:>4.4} ", "num_proc", num_proc);
      report_handler.host_value(os, "{:>5.5} ", "m_socket", socket);
      report_handler.host_value(os, "{:>5.5} ", "m_core", core);
      report_handler.host_value(os, "{:>5.5} ", "m_thread", thread);
      report_handler.host_value(os, "{:>6.6} ", "load_avg", load_avg);
      report_handler.host_value(os, "{:>7.7} ", "mem_total", mem_total);
      report_handler.host_value(os, "{:>7.7} ", "mem_used", mem_used);
      report_handler.host_value(os, "{:>7.7} ", "swap_total", swap_total);
      report_handler.host_value(os, "{:>7.7} ", "swap_used", swap_used);
   }

   sge_dstring_free(&rs);

   DRETURN_VOID;
}

/** @brief Render the queues of one host
 *
 * Sets the load and suspend alarm of each queue instance it reports.
 *
 * @param os stream to write to
 * @param host the execution host
 * @param parameter the call's parameters
 * @param model the fetched lists
 * @param report_handler the view whose hooks are called
 * @return none
 */
void
ocs::QHostViewBase::show_host_queues(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::EH> host,
                                     QHostParameter &parameter, QHostModelBase &model,
                                     QHostViewBase &report_handler) {
   DENTER(TOP_LAYER);
   uint32_t interval;
   const char *ehname = host.get<EH_name>();
   const auto qlp = model.get_queue_list();
   const auto ehl = model.get_exec_host_list();
   const auto cl = model.get_centry_list();
   uint32_t show = parameter.get_show();

   if (!(show & QHOST_DISPLAY_QUEUES) && !(show & QHOST_DISPLAY_JOBS)) {
      DRETURN_VOID;
   }

   for (const auto cqueue : qlp) {
      const auto qinstance_list = cqueue.get<CQ_qinstances>();

      if (const auto qep = qinstance_list.find_first<QU_qhostname>(ocs::tom::Host(ehname)); qep != nullptr) {
         const char *qname = qep.get<QU_qname>();

         if (show & QHOST_DISPLAY_QUEUES) {
            report_handler.queue_start(os, "   {:<20} ", qname);

            /*
            ** qtype
            */
            {
               dstring type_string = DSTRING_INIT;

               // @todo ostringstream
               qinstance_print_qtype_to_dstring(qep, &type_string, true);
               report_handler.queue_value(os, qname, "{:<5.5} ", "qtype_string",
                                          sge_dstring_get_string(&type_string));
               sge_dstring_free(&type_string);
            }

            // show reserved/used/tota slots
            report_handler.queue_value(os, qname, "{}/", "slots_resv", qinstance_slots_reserved(qep));
            report_handler.queue_value(os, qname, "{}/", "slots_used", qinstance_slots_used(qep));
            report_handler.queue_value(os, qname, "{} ", "slots", qep.get<QU_job_slots>());

            /*
            ** state of queue
            */
            const ocs::tom::ConstList<ocs::tom::CE> load_thresholds = qep.get<QU_load_thresholds>();
            const ocs::tom::ConstList<ocs::tom::CE> suspend_thresholds = qep.get<QU_suspend_thresholds>();
            if (sge_load_alarm(nullptr, 0, qep, load_thresholds, ehl, cl, ocs::tom::ConstList<ocs::tom::CE>(),
                               model.get_job_load_adjustments(), true)) {
               qinstance_state_set_alarm(qep, true);
            }
            parse_ulong_val(nullptr, &interval, CEntry::Type::TIME, qep.get<QU_suspend_interval>(), nullptr,
                            0);
            if (qep.get<QU_nsuspend>() != 0 && interval != 0
                && sge_load_alarm(nullptr, 0, qep, suspend_thresholds, ehl, cl, ocs::tom::ConstList<ocs::tom::CE>(),
                                  model.get_job_load_adjustments(), false)) {
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
            uint32_t full_listing = (show & QHOST_DISPLAY_QUEUES) ?
                                    QSTAT_DISPLAY_FULL : 0;
            full_listing = full_listing | QSTAT_DISPLAY_ALL;
            report_handler.show_jobs_per_queue(os, qep, 1, full_listing, "   ", GROUP_NO_PETASK_GROUPS, 10, parameter,
                                               model, report_handler);
         }
      }
   }

   DRETURN_VOID;
}


/** @brief Render the resources of one host
 * @param os stream to write to
 * @param host the execution host
 * @param parameter the call's parameters
 * @param model the fetched lists
 * @param report_handler the view whose hooks are called
 * @return none
 */
void
ocs::QHostViewBase::show_host_resources(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::EH> host,
                                        const QHostParameter &parameter, const QHostModelBase &model,
                                        QHostViewBase &report_handler) {
   DENTER(TOP_LAYER);

   char dom[5];
   dstring resource_string = DSTRING_INIT;
   uint32_t dominant;
   int first = 1;
   const auto ehl = model.get_exec_host_list();
   const auto cl = model.get_centry_list();
   const auto resl = parameter.get_resource_visible_list();
   uint32_t show = parameter.get_show();

   if (!(show & QHOST_DISPLAY_RESOURCES)) {
      DRETURN_VOID;
   }
   const auto rlp = host_complexes2scheduler(host, ehl, cl, model.get_job_load_adjustments());
   for (const auto rep : *rlp) {
      if (resl != nullptr) {
         int found = 0;
         for (const auto r1 : resl) {
            if (!strcmp(r1.get<ST_name>(), rep.get<CE_name>()) ||
                !strcmp(r1.get<ST_name>(), rep.get<CE_shortcut>())) {
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
      uint64_t uint64_value = 0;
      double double_value = 0.0;
      const char *s = sge_get_dominant_stringval(rep, &dominant, &resource_string, &double_value, &uint64_value);

      // find current usage for m_topology
      std::string details;
      if (strcmp(rep.get<CE_name>(), LOAD_ATTR_TOPOLOGY) == 0) {
         details = host_get_topology_in_use(host);
      }

      monitor_dominance(dom, dominant);

      const auto type = static_cast<CEntry::Type>(rep.get<CE_valtype>());
      const bool as_string = type == CEntry::Type::STR || type == CEntry::Type::CSTR || type == CEntry::Type::HOST ||
                             type == CEntry::Type::RESTR || type == CEntry::Type::HOST;
      const bool as_double = type == CEntry::Type::DOUBLE;
      const bool as_bool = type == CEntry::Type::BOOL;
      const char *details_str = details.empty() ? nullptr : details.c_str();
      if (as_string) {
         report_handler.resource_value(os, dom, rep.get<CE_name>(), s, details_str, as_string);
      } else if (as_double) {
         report_handler.resource_value(os, dom, rep.get<CE_name>(), double_value, details_str, as_string);
      } else if (as_bool) {
         report_handler.resource_value(os, dom, rep.get<CE_name>(), uint64_value ? "true" : "false",
                                       details_str, false);
      } else {
         report_handler.resource_value(os, dom, rep.get<CE_name>(), uint64_value, details_str, as_string);
      }
   }
   sge_dstring_free(&resource_string);

   DRETURN_VOID;
}

/** @brief Re-render a double that was already formatted as text
 *
 * @param new_string receives the result
 * @param result_size size of that buffer
 * @param format the format to apply
 * @param old_string the value as it was formatted before
 */
void ocs::QHostViewBase::reformat_double_string(char *new_string, const size_t result_size, const char *format, const char *old_string) {
   DENTER(TOP_LAYER);

   double dval;
   if (parse_ulong_val(&dval, nullptr, CEntry::Type::MEM, old_string, nullptr, 0)) {
      if (dval == DBL_MAX) {
         std::strcpy(new_string, INFINITY_STR);   // CS-2318: canonical uppercase token
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
/** @brief Render one job line
 *
 * The parameter list is long because the caller has already worked out
 * everything about the job's place in the listing - which slot of which
 * queue, how wide the name column is, whether the id still has to be
 * printed - and the hook only formats.
 *
 * @param os stream to write to
 * @param job the job
 * @param jatep the array task
 * @param qep the queue instance the task runs in, an empty handle if there is none
 * @param print_jobid whether the job id still has to be printed on this line
 * @param master name of the master queue for a parallel job
 * @param dyn_task_str the task range, pre-rendered
 * @param full_listing which optional columns the user asked for
 * @param slots slots the task holds in this queue
 * @param slot which slot this line is about
 * @param indent leading whitespace for this nesting level
 * @param group_opt how tasks of an array job are grouped
 * @param slots_per_line how many slots fit on one line
 * @param queue_name_length width of the queue name column
 * @param parameter the call's parameters
 * @param model the fetched lists
 * @param report_handler the view whose hooks are called
 * @return none
 */
void
ocs::QHostViewBase::show_job(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::JB> job,
                             const ocs::tom::ConstElement<ocs::tom::JAT> jatep,
                             const ocs::tom::ConstElement<ocs::tom::QU> qep, const int print_jobid,
                             const char *master, dstring *dyn_task_str, const uint32_t full_listing, const int slots,
                             const int slot, const char *indent, const uint32_t group_opt, int slots_per_line,
                             const int queue_name_length, QHostParameter &parameter, QHostModelBase &model,
                             QHostViewBase &report_handler) {
   DENTER(TOP_LAYER);
   char state_string[8];
   uint32_t jstate;
   int sge_urg, sge_pri, sge_ext, sge_time;
   ocs::tom::ConstElement<ocs::tom::JG> gdil_ep = nullptr;
   int running;
   const char *queue_name = nullptr;
   int tsk_ext;
   u_long tickets, otickets, stickets, ftickets;
   dstring ds;
   char buffer[128];
   dstring queue_name_buffer = DSTRING_INIT;
   uint32_t jid = job.get<JB_job_number>();
   const auto pe_list = model.get_pe_list();

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

   if (show_job_header_) {
      if (!(full_listing & QSTAT_DISPLAY_FULL)) {
         show_job_header_ = 0;

         oss << indent;
         oss << std::format("{:<10} ", ocs::ascii_width("job-ID"));
         oss << std::format("{:<7} ", ocs::ascii_width("prior"));
         if (sge_pri || sge_urg) {
            oss << std::format("{:<8} ", ocs::ascii_width("nurg"));
         }
         if (sge_pri) {
            oss << std::format("{:<8} ", ocs::ascii_width("npprior"));
         }
         if (sge_pri || sge_ext) {
            oss << std::format("{:<8} ", ocs::ascii_width("ntckts"));
         }
         if (sge_urg) {
            oss << std::format("{:<9} ", ocs::ascii_width("urg"));
            oss << std::format("{:<9} ", ocs::ascii_width("rrcontr"));
            oss << std::format("{:<9} ", ocs::ascii_width("wtcontr"));
            oss << std::format("{:<9} ", ocs::ascii_width("dlcontr"));
         }
         if (sge_pri) {
            oss << std::format("{:<6} ", ocs::ascii_width("ppri"));
         }
         oss << std::format("{:<10} ", ocs::ascii_width("name"));
         oss << std::format("{:<12} ", ocs::ascii_width("user"));
         if (sge_ext) {
            oss << std::format("{:<16} ", ocs::ascii_width("project"));
            oss << std::format("{:<10} ", ocs::ascii_width("department"));
         }
         oss << std::format("{:<5} ", ocs::ascii_width("state"));
         if (sge_time) {
            oss << std::format("{:<19} ", ocs::ascii_width("submit/start at"));
         }
         if (sge_urg) {
            oss << std::format("{:<19} ", ocs::ascii_width("deadline"));
         }
         if (sge_ext) {
            oss << std::format("{:<10} ", ocs::ascii_width(USAGE_ATTR_CPU));
            oss << std::format("{:<10} ", ocs::ascii_width(USAGE_ATTR_MEM));
            oss << std::format("{:<10} ", ocs::ascii_width(USAGE_ATTR_IO));
            oss << std::format("{:<6} ", ocs::ascii_width("tckts"));
            oss << std::format("{:<6} ", ocs::ascii_width("ovrts"));
            oss << std::format("{:<6} ", ocs::ascii_width("otckt"));
            oss << std::format("{:<6} ", ocs::ascii_width("ftckt"));
            oss << std::format("{:<6} ", ocs::ascii_width("stckt"));
            oss << std::format("{:<6} ", ocs::ascii_width("share"));
         }

         // dynamic queue length
         std::string fmt = std::format("{{:<{}.{}}} ", queue_name_length, queue_name_length);
         const auto queue_header = ocs::ascii_width("queue");
         oss << std::vformat(fmt, std::make_format_args(queue_header));

         oss << std::format("{:<6} ", ocs::ascii_width((group_opt & GROUP_NO_PETASK_GROUPS) ? "master" : "slots"));
         oss << std::format("{:<10} ", ocs::ascii_width("ja-task-ID"));

         if (tsk_ext) {
            oss << std::format("{:<10} ", ocs::ascii_width("task-ID"));
            oss << std::format("{:<6} ", ocs::ascii_width("state"));
            oss << std::format("{:<10} ", ocs::ascii_width(USAGE_ATTR_CPU));
            oss << std::format("{:<10} ", ocs::ascii_width(USAGE_ATTR_MEM));
            oss << std::format("{:<10} ", ocs::ascii_width(USAGE_ATTR_IO));
            oss << std::format("{:<6} ", ocs::ascii_width("stat"));
            oss << std::format("{:<7} ", ocs::ascii_width("failed"));
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
   report_handler.job_value(os, jid, "{:>10} ", nullptr, jid, false);

   /* per job priority information */
   {
      if (typeid(report_handler) == typeid(ocs::QHostViewXML) || print_jobid) {
         report_handler.job_value(os, jid, "{:<7.5f} ", "priority", jatep.get<JAT_prio>());
      } else {
         report_handler.job_value(os, jid, std::string(7 + 1, ' ').c_str(), nullptr, nullptr);
      }

      if (sge_pri || sge_urg) {
         if (print_jobid) {
            report_handler.job_value(os, jid, "{:<7.5f} ", "nurg", job.get<JB_nurg>());
         } else {
            report_handler.job_value(os, jid, std::string(7 + 1, ' ').c_str(), nullptr, nullptr);
         }
      }

      if (sge_pri) {
         if (print_jobid) {
            report_handler.job_value(os, jid, "{:<7.5f} ", "nppri", job.get<JB_nppri>());
         } else {
            report_handler.job_value(os, jid, std::string(7 + 1, ' ').c_str(), nullptr, nullptr);
         }
      }

      if (sge_pri || sge_ext) {
         if (print_jobid) {
            // reads a field of the array task from the job, as it always did; the raw access keeps that
            // behaviour, the typed one would not compile (qhost never sets these columns, see full_listing)
            report_handler.job_value(os, jid, "{:<7.5f} ", "ntix",
                                     ocs::tom::get<JAT_ntix>(static_cast<const lListElem *>(job)));
         } else {
            report_handler.job_value(os, jid, std::string(7 + 1, ' ').c_str(), nullptr, nullptr);
         }
      }

      if (sge_urg) {
         if (print_jobid) {
            constexpr double max = 99999999;

            double value = job.get<JB_urg>();
            report_handler.job_value(os, jid, value > max ? "{:<8.3g} " : "{:<8.0f ", "urg", value);
            value = job.get<JB_rrcontr>();
            report_handler.job_value(os, jid, value > max ? "{:<8.3g} " : "{:<8.0f ", "rrcontr", value);
            value = job.get<JB_wtcontr>();
            report_handler.job_value(os, jid, value > max ? "{:<8.3g} " : "{:<8.0f ", "wtcontr", value);
            value = job.get<JB_dlcontr>();
            report_handler.job_value(os, jid, value > max ? "{:<8.3g} " : "{:<8.0f ", "dlcontr", value);
         } else {
            report_handler.job_value(os, jid, std::string(4 * (8 + 1), ' ').c_str(), nullptr, nullptr);
         }
      }

      if (sge_pri) {
         if (print_jobid) {
            // @todo avoid the cast - use template method
            report_handler.job_value(os, jid, "{:<5d} ", "priority",
                                     (uint64_t)job.get<JB_priority>() - BASE_PRIORITY);
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
      report_handler.job_value(os, jid, "{:<10.10} ", "job_name", job.get<JB_job_name>());
      report_handler.job_value(os, jid, "{:<12.12} ", "job_owner", job.get<JB_owner>());
   } else {
      report_handler.job_value(os, jid, std::string(10 + 1, ' ').c_str(), nullptr, nullptr);
      report_handler.job_value(os, jid, std::string(12 + 1, ' ').c_str(), nullptr, nullptr);
   }

   if (sge_ext) {
      if (print_jobid) {
         const char *value = job.get<JB_project>();
         report_handler.job_value(os, jid, "{:<16.16} ", "project", value != nullptr ? value : "NA");
         value = job.get<JB_department>();
         report_handler.job_value(os, jid, "{:<10.10} ", "department", value != nullptr? value : "NA");
      } else {
         report_handler.job_value(os, jid, std::string(16 + 1, ' ').c_str(), nullptr, nullptr);
         report_handler.job_value(os, jid, std::string(10 + 1, ' ').c_str(), nullptr, nullptr);
      }
   }

   /* move status info into state info */
   jstate = jatep.get<JAT_state>();
   if (jatep.get<JAT_status>() == JTRANSFERING) {
      jstate |= JTRANSFERING;
      jstate &= ~JRUNNING;
   }

   if (job.get<JB_jid_predecessor_list>() || jatep.get<JAT_hold>()) {
      jstate |= JHELD;
   }

   if (jatep.get<JAT_job_restarted>()) {
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
         uint64_t value;
         if (uint64_t jat_start_time = jatep.get<JAT_start_time>(); jat_start_time != 0) {
            name = "start_time";
            value = jat_start_time;
         } else {
            name = "submit_time";
            value = job.get<JB_submission_time>();
         }
         if (typeid(report_handler) == typeid(QHostViewPlain)) {
            const char *time_string = sge_ctime64_short(value, &ds);
            report_handler.job_value(os, jid, "{} ", name, time_string);
         } else {
            report_handler.job_value(os, jid, "{} ", name, value, true);
         }
      } else {
         report_handler.job_value(os, jid, std::string(20, ' ').c_str(), nullptr, nullptr);
      }
   }

   /* is job logically running */
   running = jatep.get<JAT_status>() == JRUNNING || jatep.get<JAT_status>() == JTRANSFERING;

   /* deadline time */
   if (sge_urg) {
      uint64_t deadline = job.get<JB_deadline>();
      if (print_jobid && deadline != 0) {
         report_handler.job_value(os, jid, "{} ", "deadline", sge_ctime64_short(deadline, &ds));
      } else {
         report_handler.job_value(os, jid, std::string(20, ' ').c_str(), nullptr, nullptr);
      }
   }

   if (sge_ext) {
      ocs::tom::ConstElement<ocs::tom::PE> pe;
      const char *pe_name;

      ocs::tom::ListPtr<ocs::tom::UA> job_usage_list;
      if (!master || !strcmp(master, "MASTER")) {
         job_usage_list = jatep.get<JAT_scaled_usage_list>().clone();
      } else {
         job_usage_list = ocs::tom::ListPtr<ocs::tom::UA>::create("");
      }

      /* sum pe-task usage based on queue slots */
      if (job_usage_list != nullptr) {
         int subtask_ndx = 1;
         for (const auto task : jatep.get<JAT_task_list>()) {
            ocs::tom::ConstElement<ocs::tom::JG> ep;
            const char *qname;

            if (!slots || (queue_name && ((ep = task.get<PET_granted_destin_identifier_list>().front())) &&
                           ((qname = ep.get<JG_qname>())) && !strcmp(qname, queue_name) &&
                           ((subtask_ndx++ % slots) == slot))) {
               for (const auto src : task.get<PET_scaled_usage>()) {
                  if (const auto dst = job_usage_list->find_first<UA_name>(src.get<UA_name>());
                      dst != nullptr) {
                     dst.set<UA_value>(dst.get<UA_value>() + src.get<UA_value>());
                  } else {
                     job_usage_list->push_back(src.clone());
                  }
               }
            }
         }
      }

      /* scaled cpu usage */
      const auto up = job_usage_list->find_first<UA_name>(USAGE_ATTR_CPU);
      if (up != nullptr) {
         int secs = up.get<UA_value>();
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
      const auto up_ua = job_usage_list->find_first<UA_name>(USAGE_ATTR_MEM);
      if (up_ua != nullptr) {
         report_handler.job_value(os, jid, "{:<5.5f}", nullptr, up_ua.get<UA_value>());
      } else {
         report_handler.job_value(os, jid, "{:<7.7}", nullptr, running ? "NA" : "");
      }

      /* scaled io usage */
      const auto up_ua_2 = job_usage_list->find_first<UA_name>(USAGE_ATTR_IO);
      if (up_ua_2 != nullptr) {
         report_handler.job_value(os, jid, "{:<5.5f}", nullptr, up_ua_2.get<UA_value>());
      } else {
         report_handler.job_value(os, jid, "{:<7.7}", nullptr, running ? "NA" : "");
      }

      job_usage_list.reset();

      /* get tickets for job/slot */
      /* braces needed to suppress compiler warnings */
      if ((pe_name = jatep.get<JAT_granted_pe>()) && (pe = pe_list.find_first<PE_name>(pe_name)) &&
          pe.get<PE_control_slaves>() && slots &&
          (gdil_ep = jatep.find_first_in<JAT_granted_destin_identifier_list, JG_qname>(queue_name))) {
         if (slot == 0) {
            tickets = (u_long)gdil_ep.get<JG_ticket>();
            otickets = (u_long)gdil_ep.get<JG_oticket>();
            ftickets = (u_long)gdil_ep.get<JG_fticket>();
            stickets = (u_long)gdil_ep.get<JG_sticket>();
         } else {
            if (slots) {
               tickets = (u_long)(gdil_ep.get<JG_ticket>() / slots);
               otickets = (u_long)(gdil_ep.get<JG_oticket>() / slots);
               ftickets = (u_long)(gdil_ep.get<JG_fticket>() / slots);
               stickets = (u_long)(gdil_ep.get<JG_sticket>() / slots);
            } else {
               tickets = otickets = ftickets = stickets = 0;
            }
         }
      } else {
         tickets = (u_long)jatep.get<JAT_tix>();
         otickets = (u_long)jatep.get<JAT_oticket>();
         ftickets = (u_long)jatep.get<JAT_fticket>();
         stickets = (u_long)jatep.get<JAT_sticket>();
      }

      /* report jobs dynamic scheduling attributes */
      /* only scheduled have these attribute */
      /* Pending jobs can also have tickets */
      if (sge_ext || jatep.get<JAT_granted_destin_identifier_list>()) {
         report_handler.job_value(os, jid, "{:<5.5d} ", nullptr, tickets);
         report_handler.job_value(os, jid, "{:<5.5d} ", nullptr,
                                  (uint64_t)job.get<JB_override_tickets>());
         report_handler.job_value(os, jid, "{:<5.5d} ", nullptr, otickets);
         report_handler.job_value(os, jid, "{:<5.5d} ", nullptr, ftickets);
         report_handler.job_value(os, jid, "{:<5.5d} ", nullptr, stickets);
         report_handler.job_value(os, jid, "{:<5.2f} ", nullptr, jatep.get<JAT_share>());
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
      if (typeid(report_handler) == typeid(ocs::QHostViewXML) ||
          typeid(report_handler) == typeid(ocs::QHostViewJSON) || master) {
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
      report_handler.job_value(os, jid, "{:<6.6d} ","slots_per_line", (uint64_t)slots_per_line);
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

/** @brief Render the jobs running in one queue instance
 *
 * Tags the array tasks it finds in the queue instance.
 *
 * @param os stream to write to
 * @param qep the queue instance
 * @param print_jobs_of_queue whether the jobs are listed at all
 * @param full_listing which optional columns the user asked for
 * @param indent leading whitespace for this nesting level
 * @param group_opt how tasks of an array job are grouped
 * @param queue_name_length width of the queue name column
 * @param parameter the call's parameters
 * @param model the fetched lists
 * @param report_handler the view whose hooks are called
 * @return none
 */
void
ocs::QHostViewBase::show_jobs_per_queue(std::ostream &os, const ocs::tom::ConstElement<ocs::tom::QU> qep,
                                        const int print_jobs_of_queue, const uint32_t full_listing,
                                        const char *indent, const uint32_t group_opt, const int queue_name_length,
                                        QHostParameter &parameter, QHostModelBase &model,
                                        QHostViewBase &report_handler) {
   DENTER(TOP_LAYER);

   uint32_t job_tag;
   uint32_t jid = 0, old_jid;
   uint32_t jataskid = 0, old_jataskid;
   const char *qnm;
   dstring dyn_task_str = DSTRING_INIT;

   qnm = qep.get<QU_full_name>();

   const auto job_list = model.get_job_list();
   const auto pe_list = model.get_pe_list();

   // @todo why the dependency to the user list? Is there some code missing for qhost?
   const ocs::tom::ConstList<ocs::tom::ST> user_list{};
   for (const auto jlep : job_list) {
      int master, i;

      for (const auto jatep : jlep.get<JB_ja_tasks>()) {
         uint32_t jstate = jatep.get<JAT_state>();

         //if (shut_me_down) {
         //   DRETURN(1);
         //}

         if (ISSET(jstate, JSUSPENDED_ON_SUBORDINATE) || ISSET(jstate, JSUSPENDED_ON_SLOTWISE_SUBORDINATE)) {
            jatep.set<JAT_state>(jstate & ~JRUNNING);
         }

         const auto gdilep = jatep.get<JAT_granted_destin_identifier_list>().find_first<JG_qname>(qnm);
         if (gdilep != nullptr) {
            int slot_adjust = 0;
            int lines_to_print;
            int slots_per_line, slots_in_queue = gdilep.get<JG_slots>();

            job_tag = jatep.get<JAT_suitable>();
            job_tag |= TAG_FOUND_IT;
            jatep.set<JAT_suitable>(job_tag);

            master = !strcmp(qnm,
                             jatep.get<JAT_granted_destin_identifier_list>().front().get<JG_qname>());

            if (master) {
               const char *pe_name;
               ocs::tom::ConstElement<ocs::tom::PE> pe;
               if (((pe_name = jatep.get<JAT_granted_pe>())) &&
                   ((pe = pe_list.find_first<PE_name>(pe_name))) && !pe.get<PE_job_is_first_task>()) {
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
                  slots_per_line = sge_granted_slots(jatep.get<JAT_granted_destin_identifier_list>());
               }
            } else {
               /* yes */
               lines_to_print = (int)slots_in_queue + slot_adjust;
               slots_per_line = 1;
            }

            for (i = 0; i < lines_to_print; i++) {
               int already_printed = 0;

               if (user_list.empty() ||
                   (!user_list.empty() && (jatep.get<JAT_suitable>() & TAG_SELECT_IT))) {
                  if (print_jobs_of_queue && (job_tag & TAG_SHOW_IT)) {
                     int different, print_jobid;

                     old_jid = jid;
                     jid = jlep.get<JB_job_number>();
                     old_jataskid = jataskid;
                     jataskid = jatep.get<JAT_task_number>();
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
                         (jatep.get<JAT_state>() & JRUNNING)) {
                        report_handler.show_job(os, jlep, jatep, qep, print_jobid,
                                      (master && different && (i == 0)) ? "MASTER" : "SLAVE", &dyn_task_str,
                                      full_listing, slots_in_queue + slot_adjust, i, indent,
                                      group_opt, slots_per_line, queue_name_length, parameter, model, report_handler);
                        already_printed = 1;
                     }
                     if (!already_printed && (full_listing & QSTAT_DISPLAY_SUSPENDED) &&
                         ((jatep.get<JAT_state>() & JSUSPENDED) ||
                          (jatep.get<JAT_state>() & JSUSPENDED_ON_THRESHOLD) ||
                          (jatep.get<JAT_state>() & JSUSPENDED_ON_SUBORDINATE) ||
                          (jatep.get<JAT_state>() & JSUSPENDED_ON_SLOTWISE_SUBORDINATE))) {
                        report_handler.show_job(os, jlep, jatep, qep, print_jobid,
                                      (master && different && (i == 0)) ? "MASTER" : "SLAVE", &dyn_task_str,
                                      full_listing, slots_in_queue + slot_adjust, i, indent,
                                      group_opt, slots_per_line, queue_name_length, parameter, model, report_handler);
                        already_printed = 1;
                     }

                     if (!already_printed && (full_listing & QSTAT_DISPLAY_USERHOLD) &&
                         (jatep.get<JAT_hold>() & MINUS_H_TGT_USER)) {
                        report_handler.show_job(os, jlep, jatep, qep, print_jobid,
                                      (master && different && (i == 0)) ? "MASTER" : "SLAVE", &dyn_task_str,
                                      full_listing, slots_in_queue + slot_adjust, i, indent,
                                      group_opt, slots_per_line, queue_name_length, parameter, model, report_handler);
                        already_printed = 1;
                     }

                     if (!already_printed && (full_listing & QSTAT_DISPLAY_OPERATORHOLD) &&
                         (jatep.get<JAT_hold>() & MINUS_H_TGT_OPERATOR)) {
                        report_handler.show_job(os, jlep, jatep, qep, print_jobid,
                                      (master && different && (i == 0)) ? "MASTER" : "SLAVE", &dyn_task_str,
                                      full_listing, slots_in_queue + slot_adjust, i, indent,
                                      group_opt, slots_per_line, queue_name_length, parameter, model, report_handler);
                        already_printed = 1;
                     }

                     if (!already_printed && (full_listing & QSTAT_DISPLAY_SYSTEMHOLD) &&
                         (jatep.get<JAT_hold>() & MINUS_H_TGT_SYSTEM)) {
                        report_handler.show_job(os, jlep, jatep, qep, print_jobid,
                                      (master && different && (i == 0)) ? "MASTER" : "SLAVE", &dyn_task_str,
                                      full_listing, slots_in_queue + slot_adjust, i, indent,
                                      group_opt, slots_per_line, queue_name_length, parameter, model, report_handler);
                        already_printed = 1;
                     }

                     if (!already_printed && (full_listing & QSTAT_DISPLAY_JOBARRAYHOLD) &&
                         (jatep.get<JAT_hold>() & MINUS_H_TGT_JA_AD)) {
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


