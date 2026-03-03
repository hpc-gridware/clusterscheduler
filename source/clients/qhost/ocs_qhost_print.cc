/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this code are Copyright 2011 Univa Inc.
 *
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <climits>
#include <math.h>
#include <cfloat>
#include <sstream>
#include <iostream>
#include <format>

#include "uti/ocs_Pattern.h"
#include "uti/sge_bitfield.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

#include "comm/commlib.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_eval_expression.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_qinstance_type.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_range.h"

#include "gdi/ocs_gdi_Client.h"

#include "sched/sge_complex_schedd.h"
#include "sched/sge_select_queue.h"
#include "sched/sge_urgency.h"
#include "sched/sge_job_schedd.h"

#include "basis_types.h"
#include "sge.h"
#include "sig_handlers.h"
#include "ocs_client_print.h"
#include "ocs_qhost_print.h"

#include "msg_common.h"
#include "msg_clients_common.h"
#include "ocs_client_job.h"
#include "ocs_Bootstrap.h"
#include "ocs_QHostReportHandlerXML.h"

#define OPTI_PRINT8(output, hide_data, value)                                                                                             \
if (value > 99999999)                                                                                               \
   if (hide_data)                                                                                                    \
      sge_dstring_sprintf_append(&output, "%8s ", "*");                                                                                         \
   else                                                                                                                \
      sge_dstring_sprintf_append(&output, "%8.3g ", value);                                                                                         \
else                                                                                                                \
   if (hide_data)                                                                                                    \
      sge_dstring_sprintf_append(&output, "%8s ", "*");                                                                                         \
   else                                                                                                                \
      sge_dstring_sprintf_append(&output, "%8.0f ", value)

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

static int sge_print_queues(std::ostream &os, lList *ql, lListElem *hrl, lList *jl, lList *ul, lList *ehl, lList *cl,
                            lList *pel, lList *acll, u_long32 show, ocs::QHostReportHandlerXML *report_handler, lList **alpp, bool is_manager);
static int sge_print_resources(std::ostream &os, lList *ehl, lList *cl, lList *resl, lList *acl_list, lListElem *host, u_long32 show, ocs::QHostReportHandlerXML *report_handler, lList **alpp);
static int sge_print_host(std::ostream &os, lListElem *hep, lList *centry_list, lList *acl_list, ocs::QHostReportHandlerXML *report_handler, lList **alpp, u_long32 show, bool is_manager);

static int reformatDoubleValue(char *result, size_t result_size, const char *format, const char *oldmem);
static bool get_all_lists(lList **answer_list, lList **ql, lList **jl, lList **cl, lList **ehl, lList **pel, lList **acll, bool *is_manager, lList *hl, lList *ul, u_long32 show);
static void free_all_lists(lList **ql, lList **jl, lList **cl, lList **ehl, lList **pel, lList **acll);


bool
qinstance_is_visible(const lListElem *qep, bool is_manager, bool dept_view, const lList *acl_list) {
   bool qinstance_visible = true;

   if (!is_manager && dept_view) {
      const char *username = component_get_username();
      const char *groupname = component_get_groupname();
      int amount;
      ocs_grp_elem_t *grp_array;
      component_get_supplementray_groups(&amount, &grp_array);
      lList *grp_list = grp_list_array2list(amount, grp_array);
      qinstance_visible = sge_has_access(username, groupname, grp_list, qep, acl_list);
      lFreeList(&grp_list);
   }
   return qinstance_visible;
}

int do_qhost(lList *host_list, lList *user_list, lList *resource_match_list,
              lList *resource_list, u_long32 show, lList **alpp, ocs::QHostReportHandlerXML *report_handler) {

   lList *cl = nullptr;
   lList *ehl = nullptr;
   lList *ql = nullptr;
   lList *jl = nullptr;
   lList *pel = nullptr;
   lList *acll = nullptr;
   lListElem *ep;
   lCondition *where = nullptr;
   bool have_lists = true;
   int print_header = 1;
   int ret = QHOST_SUCCESS;
   bool show_binding = ((show & QHOST_DISPLAY_BINDING) == QHOST_DISPLAY_BINDING) ? true : false;
   bool is_manager = false;
#define HEAD_FORMAT_DEPT "%-23s %-13s%4s %5s %5s %5s %6s %7s %7s %7s %7s\n"
#define HEAD_FORMAT "%-23s %-13.13s%4.4s %5.5s %5.5s %5.5s %6.6s %7.7s %7.7s %7.7s %7.7s\n"
#define HEAD_FORMAT_OLD_DEPT "%-23s %-13s%4s %5s %7s %7s %7s %7s\n"
#define HEAD_FORMAT_OLD "%-23s %-13.13s%4.4s %6.6s %7.7s %7.7s %7.7s %7.7s\n"
   
   DENTER(TOP_LAYER);
   
   have_lists = get_all_lists(alpp, &ql, &jl, &cl, &ehl, &pel, &acll, &is_manager, host_list, user_list, show);
   if (!have_lists) {
      free_all_lists(&ql, &jl, &cl, &ehl, &pel, &acll);
      DRETURN(QHOST_ERROR);
   }

   /* 
   ** delete ok message 
   */
   lFreeList(alpp);


   centry_list_init_double(cl);

   /*
   ** handle -l request for host
   */
   if (lGetNumberOfElem(resource_match_list)) {
      int selected;
      lListElem *global = nullptr;

      if (centry_list_fill_request(resource_match_list, alpp, cl, true, true, false)) {
         /* TODO: error message gets written by centry_list_fill_request into SGE_EVENT */
         free_all_lists(&ql, &jl, &cl, &ehl, &pel, &acll);
         DRETURN(QHOST_ERROR);
      }

      {/* clean host list */
         lListElem *host = nullptr;
         for_each_rw(host, ehl) {
            lSetUlong(host, EH_tagged, 0);
         }
      }

      /* prepare request */
      global = lGetElemHostRW(ehl, EH_name, "global");
      selected = sge_select_queue(resource_match_list, nullptr, global, ehl, cl, true, -1, nullptr, nullptr, nullptr);
      if (selected) {
         for_each_rw(ep, ehl) {
            lSetUlong(ep, EH_tagged, 1);
         }
      } else {
        lListElem *tmp_resource_list = nullptr;
        /* if there is hostname request, remove it as we cannot match hostname in
         * sge_select_queue
         * we'll process this tmp_resource_list separately!!
         */
         if ((tmp_resource_list = lGetElemStrRW(resource_match_list, CE_name, "hostname"))) {
            lDechainElem(resource_match_list, tmp_resource_list);
         }
         for_each_rw(ep, ehl) {
            /* prepare complex attributes */
            if (strcmp(lGetHost(ep, EH_name), SGE_TEMPLATE_NAME) == 0) {
               continue;
            }

            DPRINTF("matching host %s with qhost -l\n", lGetHost(ep, EH_name));

            selected = sge_select_queue(resource_match_list, nullptr, ep, ehl, cl,
                                        true, -1, nullptr, nullptr, nullptr);
            if(selected) { /* found other matching attribs */
               lSetUlong(ep, EH_tagged, 1);
               /* check for hostname match if there was a hostname match request */
               if (tmp_resource_list != nullptr) {
                  if (sge_hostcmp(lGetString(tmp_resource_list, CE_stringval), lGetHost(ep, EH_name)) != 0 ) {
                     DPRINTF("NOT matched hostname %s with qhost -l\n", lGetHost(ep, EH_name));
                     lSetUlong(ep, EH_tagged, 0);
                  }
               }
            } else { /* this host is not selected */
               lSetUlong(ep, EH_tagged, 0);
            }
         }
         if (tmp_resource_list != nullptr) {
            lFreeElem(&tmp_resource_list);
         }
      }

      /*
      ** reduce the hostlist, only the tagged ones survive
      */
      where = lWhere("%T(%I == %u)", EH_Type, EH_tagged, 1);
      lSplit(&ehl, nullptr, nullptr, where);
      lFreeWhere(&where);
   }

   /* scale load values and adjust consumable capacities */
   /*    TODO                                            */
   /*    is correct_capacities needed here ???           */
   /*    correct_capacities(ehl, cl);                    */

   /* SGE_GLOBAL_NAME should be printed at first */
   lPSortList(ehl, "%I+", EH_name);
   ep = nullptr;
   where = lWhere("%T(%I == %s)", EH_Type, EH_name, SGE_GLOBAL_NAME );
   ep = lDechainElem(ehl, lFindFirstRW(ehl, where));
   lFreeWhere(&where);
   if (ep) {
      lInsertElem(ehl, nullptr, ep);
   }
   
   /*
   ** output handling
   */
   std::ostringstream oss;
   if (report_handler != nullptr) {
      report_handler->start(oss);
   }
   for_each_rw(ep, ehl) {
      bool dept_view = ((show & QHOST_DISPLAY_DEPT_VIEW) == QHOST_DISPLAY_DEPT_VIEW) ? true : false;
      bool hide_data = !host_is_visible(ep, is_manager, dept_view, acll);

      if (hide_data) {
         continue;
      }

      if (shut_me_down) {
         DRETURN(QHOST_ERROR);
      }

      if (report_handler == nullptr ) {
         if (print_header) {
            print_header = 0;
            if (show_binding) {
               printf(HEAD_FORMAT,  MSG_HEADER_HOSTNAME, MSG_HEADER_ARCH, MSG_HEADER_NPROC,
                  MSG_HEADER_NSOC, MSG_HEADER_NCOR, MSG_HEADER_NTHR, MSG_HEADER_LOAD, MSG_HEADER_MEMTOT,
                  MSG_HEADER_MEMUSE, MSG_HEADER_SWAPTO, MSG_HEADER_SWAPUS);
            } else {
               printf(HEAD_FORMAT_OLD,  MSG_HEADER_HOSTNAME, MSG_HEADER_ARCH, MSG_HEADER_NPROC, 
                  MSG_HEADER_LOAD, MSG_HEADER_MEMTOT, MSG_HEADER_MEMUSE, MSG_HEADER_SWAPTO, 
                  MSG_HEADER_SWAPUS);
            }
            printf("--------------------------------------------------------------------------------");
            if (show_binding) {
               printf("------------------");
            } 
            printf("\n");
         }
      } else {
         report_handler->host_start(oss, lGetHost(ep, EH_name));
      }
      sge_print_host(oss, ep, cl, acll, report_handler, alpp, show, is_manager);
      sge_print_resources(oss, ehl, cl, resource_list, acll, ep, show, report_handler, alpp);

      ret = sge_print_queues(oss, ql, ep, jl, nullptr, ehl, cl, pel, acll, show, report_handler, alpp, is_manager);
      if (ret != QHOST_SUCCESS) {
         break;
      }
      
      if (report_handler != nullptr) {
         DPRINTF("report host_finished: %s\n", lGetHost(ep, EH_name));
         report_handler->host_end(oss);
      }
   }   
   if (report_handler != nullptr) {
      report_handler->end(oss);
   }

   std::cout << oss.str();
   free_all_lists(&ql, &jl, &cl, &ehl, &pel, &acll);
   DRETURN(ret);
}

/*-------------------------------------------------------------------------*/
static int 
sge_print_host(std::ostream &os, lListElem *hep, lList *centry_list, lList *acl_list, ocs::QHostReportHandlerXML *report_handler,
               lList **alpp, u_long32 show, bool is_manager)
{
   lListElem *lep;
   char *s, host_print[CL_MAXHOSTNAMELEN+1] = "";
   const char *host;
   char load_avg[20], mem_total[20], mem_used[20], swap_total[20],
        swap_used[20], num_proc[20], socket[20], core[20], arch_string[80], thread[20];
   dstring rs = DSTRING_INIT;
   u_long32 dominant = 0;
   int ret = QHOST_SUCCESS;
   bool ignore_fqdn = ocs::Bootstrap::get_ignore_fqdn();
   bool show_binding = ((show & QHOST_DISPLAY_BINDING) == QHOST_DISPLAY_BINDING) ? true : false;
   bool dept_view = ((show & QHOST_DISPLAY_DEPT_VIEW) == QHOST_DISPLAY_DEPT_VIEW) ? true : false;
   bool hide_data = !host_is_visible(hep, is_manager, dept_view, acl_list);

   DENTER(TOP_LAYER);

   host = lGetHost(hep, EH_name);

   /* cut away domain in case of ignore_fqdn */
   sge_strlcpy(host_print, host, CL_MAXHOSTNAMELEN);
   if (ignore_fqdn && (s = strchr(host_print, '.'))) {
      *s = '\0';
   }   

   /*
   ** arch
   */
   lep= get_attribute_by_name(nullptr, hep, nullptr, LOAD_ATTR_ARCH, centry_list, nullptr, DISPATCH_TIME_NOW, 0);
   if (lep) {
      sge_strlcpy(arch_string, sge_get_dominant_stringval(lep, &dominant, &rs), sizeof(arch_string));
      sge_dstring_clear(&rs);
      lFreeElem(&lep);
   } else {
      strcpy(arch_string, "-");
   }

   /*
   ** num_proc
   */
   lep= get_attribute_by_name(nullptr, hep, nullptr, "num_proc", centry_list, nullptr, DISPATCH_TIME_NOW, 0);
   if (lep) {
      sge_strlcpy(num_proc, sge_get_dominant_stringval(lep, &dominant, &rs), sizeof(num_proc));
      sge_dstring_clear(&rs);
      lFreeElem(&lep);
   } else {
      strcpy(num_proc, "-");
   }

   if (show_binding) {
      /*
      ** nsoc (sockets)
      */
      lep= get_attribute_by_name(nullptr, hep, nullptr, "m_socket", centry_list, nullptr, DISPATCH_TIME_NOW, 0);
      if (lep) {
         sge_strlcpy(socket, sge_get_dominant_stringval(lep, &dominant, &rs),
                  sizeof(socket));
         sge_dstring_clear(&rs);
         lFreeElem(&lep);
      } else {
         strcpy(socket, "-");
      }

      /*
      ** nthr (threads)
      */
      lep= get_attribute_by_name(nullptr, hep, nullptr, "m_thread", centry_list, nullptr, DISPATCH_TIME_NOW, 0);
      if (lep) {
         sge_strlcpy(thread, sge_get_dominant_stringval(lep, &dominant, &rs),
                  sizeof(thread));
         sge_dstring_clear(&rs);
         lFreeElem(&lep);
      } else {
         strcpy(thread, "-");
      }


      /*
      ** ncor (cores)
      */
      lep= get_attribute_by_name(nullptr, hep, nullptr, "m_core", centry_list, nullptr, DISPATCH_TIME_NOW, 0);
      if (lep) {
         sge_strlcpy(core, sge_get_dominant_stringval(lep, &dominant, &rs),
                  sizeof(core)); 
         sge_dstring_clear(&rs);
         lFreeElem(&lep);
      } else {
         strcpy(core, "-");
      }
   }

   /*
   ** load_avg
   */
   lep= get_attribute_by_name(nullptr, hep, nullptr, "load_avg", centry_list, nullptr, DISPATCH_TIME_NOW, 0);
   if (lep) {
      reformatDoubleValue(load_avg, sizeof(load_avg), "%.2f%c", sge_get_dominant_stringval(lep, &dominant, &rs));
      sge_dstring_clear(&rs);
      lFreeElem(&lep);
   } else {
      strcpy(load_avg, "-");
   }

   /*
   ** mem_total
   */
   lep= get_attribute_by_name(nullptr, hep, nullptr, "mem_total", centry_list, nullptr, DISPATCH_TIME_NOW, 0);
   if (lep) {
      reformatDoubleValue(mem_total, sizeof(mem_total), "%.1f%c", sge_get_dominant_stringval(lep, &dominant, &rs));
      sge_dstring_clear(&rs);
      lFreeElem(&lep);
   } else {
      strcpy(mem_total, "-");
   }

   /*
   ** mem_used
   */
   lep= get_attribute_by_name(nullptr, hep, nullptr, "mem_used", centry_list, nullptr, DISPATCH_TIME_NOW, 0);
   if (lep) {
      reformatDoubleValue(mem_used, sizeof(mem_used), "%.1f%c", sge_get_dominant_stringval(lep, &dominant, &rs));
      sge_dstring_clear(&rs);
      lFreeElem(&lep);
   } else {
      strcpy(mem_used, "-");
   }

   /*
   ** swap_total
   */
   lep= get_attribute_by_name(nullptr, hep, nullptr, "swap_total", centry_list, nullptr, DISPATCH_TIME_NOW, 0);
   if (lep) {
      reformatDoubleValue(swap_total, sizeof(swap_total), "%.1f%c", sge_get_dominant_stringval(lep, &dominant, &rs));
      sge_dstring_clear(&rs);
      lFreeElem(&lep);
   } else {
      strcpy(swap_total, "-");
   }

   /*
   ** swap_used
   */
   lep= get_attribute_by_name(nullptr, hep, nullptr, "swap_used", centry_list, nullptr, DISPATCH_TIME_NOW, 0);
   if (lep) {
      reformatDoubleValue(swap_used, sizeof(swap_used), "%.1f%c", sge_get_dominant_stringval(lep, &dominant, &rs));
      sge_dstring_clear(&rs);
      lFreeElem(&lep);
   } else {
      strcpy(swap_used, "-");
   }
   

   if (report_handler) {
      report_handler->host_value(os, "arch_string", arch_string);
      report_handler->host_value(os, "num_proc", num_proc);
      if (show_binding) {
         report_handler->host_value(os, "m_socket", socket);
         report_handler->host_value(os, "m_core", core);
         report_handler->host_value(os, "m_thread", thread);
      }
      report_handler->host_value(os, "load_avg", load_avg);
      report_handler->host_value(os, "mem_total", mem_total);
      report_handler->host_value(os, "mem_used", mem_used);
      report_handler->host_value(os, "swap_total", swap_total);
      report_handler->host_value(os, "swap_used", swap_used);
   } else {
      DSTRING_STATIC(output, 256);

      if (show_binding) {
         if (hide_data) {
            sge_dstring_sprintf(&output, HEAD_FORMAT_DEPT, "*", "*", "*", "*", "*", "*", "*", "*", "*", "*", "*");
         } else {
            sge_dstring_sprintf(&output, HEAD_FORMAT, host ? host_print: "-", arch_string, num_proc, socket,
                                core, thread, load_avg, mem_total, mem_used, swap_total, swap_used);
         }
      } else {
         if (hide_data) {
            sge_dstring_sprintf(&output, HEAD_FORMAT_OLD_DEPT, "*", "*", "*", "*", "*", "*", "*", "*");
         } else {
            sge_dstring_sprintf(&output, HEAD_FORMAT_OLD, host ? host_print: "-", arch_string, num_proc, load_avg,
                                mem_total, mem_used, swap_total, swap_used);
         }
      }
      os << sge_dstring_get_string(&output);
   }
   
   sge_dstring_free(&rs);
   
   DRETURN(ret);
}

/*-------------------------------------------------------------------------*/
static int sge_print_queues(
std::ostream &os,
lList *qlp,
lListElem *host,
lList *jl,
lList *ul,
lList *ehl,
lList *cl,
lList *pel,
lList *acl_list,
u_long32 show,
ocs::QHostReportHandlerXML *report_handler,
lList **alpp,
bool is_manager
) {
   const lList *load_thresholds, *suspend_thresholds;
   lListElem *qep;
   lListElem *cqueue;
   u_long32 interval;
   int ret = QHOST_SUCCESS;
   const char *ehname = lGetHost(host, EH_name);
   bool dept_view = ((show & QHOST_DISPLAY_DEPT_VIEW) == QHOST_DISPLAY_DEPT_VIEW) ? true : false;

   DENTER(TOP_LAYER);

   if (!(show & QHOST_DISPLAY_QUEUES) &&
       !(show & QHOST_DISPLAY_JOBS)) {
      DRETURN(ret);
   }

   for_each_rw(cqueue, qlp) {
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);

      if ((qep=lGetElemHostRW(qinstance_list, QU_qhostname, ehname))) {
         dstring buffer = DSTRING_INIT;
         const char *qname = lGetString(qep, QU_qname);
         bool hide_data = !qinstance_is_visible(qep, is_manager, dept_view, acl_list);

         if (hide_data) {
            continue;
         }

         if (show & QHOST_DISPLAY_QUEUES) {
            if (report_handler == nullptr) {
               if (hide_data) {
                  sge_dstring_sprintf(&buffer, "   %-20s ", "*");
               } else {
                  sge_dstring_sprintf(&buffer, "   %-20s ", qname);
               }
            } else {
               report_handler->queue_start(os, qname);
            }
            /*
            ** qtype
            */
            {
               dstring type_string = DSTRING_INIT;

               qinstance_print_qtype_to_dstring(qep, &type_string, true);
               if (report_handler == nullptr) {
                  if (hide_data) {
                     sge_dstring_sprintf_append(&buffer, "%-5s ", "*");
                  } else {
                     sge_dstring_sprintf_append(&buffer, "%-5.5s ", sge_dstring_get_string(&type_string));
                  }
               } else {
                  report_handler->queue_value(os, qname, "qtype_string", sge_dstring_get_string(&type_string));
               }                        
               sge_dstring_free(&type_string);
               
               if (ret != QHOST_SUCCESS) {
                  DRETURN(ret);
               }
            }

            /* 
            ** number of used/free slots 
            */
            if (report_handler == nullptr) {
               char buf[80];
               snprintf(buf, sizeof(buf), sge_u32"/%d/" sge_u32 " ", qinstance_slots_reserved(qep),
                        qinstance_slots_used(qep), lGetUlong(qep, QU_job_slots));
               if (hide_data) {
                  sge_dstring_sprintf_append(&buffer, "%-14s", "*/*/*");
               } else {
                  sge_dstring_sprintf_append(&buffer, "%-14.14s", buf);
               }
            } else {
               report_handler->queue_value(os, qname, "slots_used", qinstance_slots_used(qep));
               report_handler->queue_value(os, qname, "slots", lGetUlong(qep, QU_job_slots));
               report_handler->queue_value(os, qname, "slots_resv", qinstance_slots_reserved(qep));
            }
            
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

               qinstance_state_append_to_dstring(qep, &state_string);
               if (report_handler == nullptr) {
                  if (hide_data) {
                     sge_dstring_sprintf_append(&buffer, "%s", "*");
                  } else {
                     sge_dstring_sprintf_append(&buffer,  "%s", sge_dstring_get_string(&state_string));
                  }
               } else {
                  report_handler->queue_value(os, qname, "state_string", sge_dstring_get_string(&state_string));
               }
               sge_dstring_free(&state_string);
               if (ret != QHOST_SUCCESS ) {
                  DRETURN(ret);
               }
            }
            
            /*
            ** newline
            */
            if (report_handler == nullptr) {
               os << sge_dstring_get_string(&buffer) << std::endl;
               sge_dstring_free(&buffer);
            } else {
               report_handler->queue_end(os);
            }
         }

         /*
         ** tag all jobs, we have only fetched running jobs, so every job
         ** should be visible (necessary for the qstat printing functions)
         */
         if (show & QHOST_DISPLAY_JOBS) {
            u_long32 full_listing = (show & QHOST_DISPLAY_QUEUES) ?  
                                    QSTAT_DISPLAY_FULL : 0;
            full_listing = full_listing | QSTAT_DISPLAY_ALL;
            if (sge_print_jobs_queue(os, qep, jl, pel, ul, ehl, cl, 1,
                                 full_listing, "   ", 
                                 GROUP_NO_PETASK_GROUPS, 10,
                                 report_handler, alpp, show, is_manager) == 1) {
               DRETURN(QHOST_ERROR);
            }
         }
      }
   }

   DRETURN(ret);
}


/*-------------------------------------------------------------------------*/
static int sge_print_resources(
std::ostream &os,
lList *ehl,
lList *cl,
lList *resl,
lList *acl_list,
lListElem *host,
u_long32 show,
ocs::QHostReportHandlerXML *report_handler,
lList **alpp
) {
   DENTER(TOP_LAYER);

   lList *rlp = nullptr;
   lListElem *rep;
   char dom[5];
   dstring resource_string = DSTRING_INIT;
   const char *s;
   u_long32 dominant;
   int first = 1;
   int ret = QHOST_SUCCESS;
   bool dept_view = ((show & QHOST_DISPLAY_DEPT_VIEW) == QHOST_DISPLAY_DEPT_VIEW) ? true : false;
   bool hide_data = false;

   // does the executing qstat user have access to this queue?
   if (dept_view) {
      const char *username = component_get_username();
      const char *groupname = component_get_groupname();
      int amount;
      ocs_grp_elem_t *grp_array;
      component_get_supplementray_groups(&amount, &grp_array);
      lList *grp_list = grp_list_array2list(amount, grp_array);
      hide_data = !sge_has_access_(username, groupname, grp_list, lGetList(host, EH_acl), lGetList(host, EH_xacl), acl_list);
      lFreeList(&grp_list);
   }

   if (!(show & QHOST_DISPLAY_RESOURCES)) {
      DRETURN(QHOST_SUCCESS);
   }
   host_complexes2scheduler(&rlp, host, ehl, cl);
   for_each_rw(rep, rlp) {
      dstring output = DSTRING_INIT;

      if (resl != nullptr) {
         lListElem *r1;
         int found = 0;
         for_each_rw (r1, resl) {
            if (!strcmp(lGetString(r1, ST_name), lGetString(rep, CE_name)) ||
                !strcmp(lGetString(r1, ST_name), lGetString(rep, CE_shortcut))) {
               found = 1;
               if (first) {
                  first = 0;
                  if (report_handler == nullptr ) {
                     sge_dstring_sprintf_append(&output, "    Host Resource(s):   \n");
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
      if (report_handler == nullptr) {
         if (hide_data) {
            sge_dstring_sprintf_append(&output, "\t*:*=*");
         } else {
            if (details.empty()) {
               sge_dstring_sprintf_append(&output, "\t%s:%s=%s", dom, lGetString(rep, CE_name), s);
            } else {
               sge_dstring_sprintf_append(&output, "\t%s:%s=%s (%s)", dom, lGetString(rep, CE_name), s, details.c_str());
            }
         }
         os << sge_dstring_get_string(&output) << std::endl;
      } else {
         report_handler->resource_value(os, dom, lGetString(rep, CE_name), s);
      }

      sge_dstring_free(&output);

      if (ret != QHOST_SUCCESS) {
         break;
      }
   }
   lFreeList(&rlp);
   sge_dstring_free(&resource_string);

   DRETURN(ret);
}

/*-------------------------------------------------------------------------*/

static int
reformatDoubleValue(char *result, size_t result_size, const char *format, const char *oldmem)
{
   double dval;
   int ret = 1;

   DENTER(TOP_LAYER);

   if (parse_ulong_val(&dval, nullptr, TYPE_MEM, oldmem, nullptr, 0)) {
      if (dval==DBL_MAX) {
         strcpy(result, "infinity");
      } else {
         int c = '\0';

         if (fabs(dval) >= 1024*1024*1024) {
            dval /= 1024*1024*1024;
            c = 'G';
         } else if (fabs(dval) >= 1024*1024) {
            dval /= 1024*1024;
            c = 'M';
         } else if (fabs(dval) >= 1024) {
            dval /= 1024;
            c = 'K';
         }
         snprintf(result, result_size, format, dval, c);
      }
   } else {
      strcpy(result, "?E"); 
      ret = 0;
   }

   DRETURN(ret);
}

/****
 **** get_all_lists (static)
 ****
 **** Gets copies of queue-, job-, complex-, exechost-list  
 **** from qmaster.
 **** The lists are stored in the .._l pointerpointer-parameters.
 **** WARNING: Lists previously stored in this pointers are not destroyed!!
 ****/
static bool
get_all_lists(lList **answer_list, lList **queue_l, lList **job_l, lList **centry_l, lList **exechost_l,
              lList **pe_l, lList **acl_l, bool *is_manager, lList *hostref_list, lList *user_list, u_long32 show) {
   lCondition *where= nullptr, *nw = nullptr, *jw = nullptr, *gc_where;
   lEnumeration *q_all = nullptr, *j_all = nullptr, *ce_all = nullptr,
                *eh_all = nullptr, *pe_all = nullptr, *acl_all = nullptr, *gc_what;
   lListElem *ep = nullptr;
   lListElem *jatep = nullptr;
   lList *conf_l = nullptr;
   int q_id = 0, j_id = 0, ce_id, eh_id, pe_id, gc_id, acl_id;
   ocs::gdi::Request gdi_multi{};
   const char *cell_root = bootstrap_get_cell_root();
   u_long32 progid = component_get_component_id();

   DENTER(TOP_LAYER);

   // @todo Should be combined with the other GDI requests
   bool perm_return = ocs::gdi::Client::sge_gdi_get_permission(answer_list, is_manager, nullptr, nullptr, nullptr);
   if (!perm_return) {
      DRETURN(false);
   }

   /*
   ** exechosts
   ** build where struct to filter out  either all hosts or only the
   ** hosts listed in host_list
   */
   for_each_rw(ep, hostref_list) {
      nw = lWhere("%T(%I h= %s)", EH_Type, EH_name, lGetString(ep, ST_name));
      if (!where)
         where = nw;
      else
         where = lOrWhere(where, nw);
   }
   /* the global host has to be retrieved as well */
   if (where != nullptr) {
      nw = lWhere("%T(%I == %s)", EH_Type, EH_name, SGE_GLOBAL_NAME);
      where = lOrWhere(where, nw);
   }

   nw = lWhere("%T(%I != %s)", EH_Type, EH_name, SGE_TEMPLATE_NAME);
   if (where)
      where = lAndWhere(where, nw);
   else
      where = nw;
   eh_all = lWhat("%T(ALL)", EH_Type);

   eh_id = gdi_multi.request(answer_list, ocs::Mode::RECORD, ocs::gdi::Target::TargetValue::SGE_EH_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, where, eh_all, true);
   lFreeWhat(&eh_all);
   lFreeWhere(&where);

   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   if (show & QHOST_DISPLAY_JOBS || show & QHOST_DISPLAY_QUEUES) {
      q_all = lWhat("%T(ALL)", QU_Type);

      q_id = gdi_multi.request(answer_list, ocs::Mode::RECORD, ocs::gdi::Target::TargetValue::SGE_CQ_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, q_all, true);
      lFreeWhat(&q_all);

      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   /*
   ** jobs
   */
   if (job_l && (show & QHOST_DISPLAY_JOBS)) {

/* lWriteListTo(user_list, stdout); */

      for_each_rw(ep, user_list) {
         const char *user_name = lGetString(ep, ST_name);
         if (ocs::is_pattern(user_name)) {
            nw = lWhere("%T(%I p= %s)", JB_Type, JB_owner, user_name);
         } else {
            nw = lWhere("%T(%I == %s)", JB_Type, JB_owner, user_name);
         }
         if (!jw) {
            jw = nw;
         } else {
            jw = lOrWhere(jw, nw);
         }
      }
/* printf("-------------------------------------\n"); */
/* lWriteWhereTo(jw, stdout); */
      if (!(show & QSTAT_DISPLAY_PENDING)) {
         nw = lWhere("%T(%I->%T(!(%I m= %u)))", JB_Type, JB_ja_tasks, JAT_Type, JAT_state, JQUEUED);
         if (!jw)
            jw = nw;
         else
            jw = lAndWhere(jw, nw);
      }

      j_all = lWhat("%T(%I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I)", JB_Type,
                     JB_job_number,
                     JB_script_file,
                     JB_owner,
                     JB_group,
                     JB_type,
                     JB_pe,
                     JB_checkpoint_name,
                     JB_jid_predecessor_list,
                     JB_env_list,
                     JB_priority,
                     JB_jobshare,
                     JB_job_name,
                     JB_project,
                     JB_department,
                     JB_submission_time,
                     JB_deadline,
                     JB_override_tickets,
                     JB_pe_range,
                     JB_request_set_list,
                     JB_ja_structure,
                     JB_ja_tasks,
                     JB_ja_n_h_ids,
                     JB_ja_u_h_ids,
                     JB_ja_s_h_ids,
                     JB_ja_o_h_ids,
                     JB_ja_a_h_ids,
                     JB_ja_z_ids
                    );

      j_id = gdi_multi.request(answer_list, ocs::Mode::RECORD, ocs::gdi::Target::SGE_JB_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, jw, j_all, true);
      lFreeWhat(&j_all);
      lFreeWhere(&jw);

      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   /*
   ** complexes
   */
   ce_all = lWhat("%T(ALL)", CE_Type);
   ce_id = gdi_multi.request(answer_list, ocs::Mode::RECORD, ocs::gdi::Target::SGE_CE_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, ce_all, true);
   lFreeWhat(&ce_all);

   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   /*
   ** pe list
   */
   pe_all = lWhat("%T(ALL)", PE_Type);
   pe_id = gdi_multi.request(answer_list, ocs::Mode::RECORD, ocs::gdi::Target::SGE_PE_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, pe_all, true);
   lFreeWhat(&pe_all);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   /*
   ** user list
   */
   acl_all = lWhat("%T(ALL)", US_Type);
   acl_id = gdi_multi.request(answer_list, ocs::Mode::RECORD, ocs::gdi::Target::SGE_US_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, nullptr, acl_all, true);
   lFreeWhat(&acl_all);

   if (answer_list_has_error(answer_list)) {
      DRETURN(1);
   }

   /*
   ** global cluster configuration
   */
   gc_where = lWhere("%T(%I c= %s)", CONF_Type, CONF_name, SGE_GLOBAL_NAME);
   gc_what = lWhat("%T(ALL)", CONF_Type);

   gc_id = gdi_multi.request(answer_list, ocs::Mode::SEND, ocs::gdi::Target::SGE_CONF_LIST, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, gc_where, gc_what, true);
   gdi_multi.wait();
   lFreeWhat(&gc_what);
   lFreeWhere(&gc_where);

   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   /*
   ** handle results
   */
   /* --- exec host */
   gdi_multi.get_response(answer_list, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, ocs::gdi::Target::SGE_EH_LIST, eh_id, exechost_l);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   /* --- queue */
   if (show & QHOST_DISPLAY_JOBS || show & QHOST_DISPLAY_QUEUES) {
      gdi_multi.get_response(answer_list, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, ocs::gdi::Target::SGE_CQ_LIST, q_id, queue_l);
      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   /* --- job */
   if (job_l && (show & QHOST_DISPLAY_JOBS)) {
      lListElem *ep = nullptr;
      gdi_multi.get_response(answer_list, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, ocs::gdi::Target::SGE_JB_LIST, j_id, job_l);
      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
      /*
      ** tag the jobs, we need it for the printing functions
      */
      for_each_rw(ep, *job_l) {
         for_each_rw(jatep, lGetList(ep, JB_ja_tasks)) {
            lSetUlong(jatep, JAT_suitable, TAG_SHOW_IT);
         }
      }   

   }

   /* --- complex attribute */
   gdi_multi.get_response(answer_list, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, ocs::gdi::Target::SGE_CE_LIST, ce_id, centry_l);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   /* --- pe */
   gdi_multi.get_response(answer_list, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, ocs::gdi::Target::SGE_PE_LIST, pe_id, pe_l);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   /* --- user lists */
   gdi_multi.get_response(answer_list, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, ocs::gdi::Target::SGE_US_LIST, acl_id, acl_l);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   /* --- apply global configuration for sge_hostcmp() scheme */
   gdi_multi.get_response(answer_list, ocs::gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE, ocs::gdi::Target::SGE_CONF_LIST, gc_id, &conf_l);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }
   if (lFirst(conf_l)) {
      lListElem *local = nullptr;
      merge_configuration(nullptr, progid, cell_root, lFirstRW(conf_l), local, nullptr);
   }
   
   lFreeList(&conf_l);
   DRETURN(true);
}

/****** sge_qhost/free_all_lists() *********************************************
*  NAME
*     free_all_lists() -- frees all lists
*
*  SYNOPSIS
*     static void free_all_lists(lList **ql, lList **jl, lList **cl, lList 
*     **ehl, lList **pel) 
*
*  FUNCTION
*     The function frees all of the given list pointers
*
*  INPUTS
*     lList **ql  - ??? 
*     lList **jl  - ??? 
*     lList **cl  - ??? 
*     lList **ehl - ??? 
*     lList **pel - ??? 
*
*  NOTES
*     MT-NOTE: free_all_lists() is MT safe 
*******************************************************************************/
static void free_all_lists(lList **ql, lList **jl, lList **cl, lList **ehl, lList **pel, lList **acll)
{
   lFreeList(ql);
   lFreeList(jl);
   lFreeList(cl);
   lFreeList(ehl);
   lFreeList(pel);
   lFreeList(acll);
}

static int sge_print_subtask(std::ostream &os, const lListElem *job, const lListElem *ja_task,
                             const lListElem *pe_task, /* nullptr, if master task shall be printed */
                             int print_hdr, int indent) {
   char task_state_string[8];
   u_long32 tstate, tstatus;
   int task_running;
   const char *str;
   const lListElem *ep;
   const lList *usage_list;
   const lList *scaled_usage_list;

   DENTER(TOP_LAYER);

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

   task_running = (tstatus == JRUNNING || tstatus == JTRANSFERING);

   if (print_hdr) {
      os << std::format("{}Sub-tasks:           {:<12.12} {:>5.5} {} {:<4.4} {:<6.6}\n",
                        QSTAT_INDENT, "task-ID", "state", std::string(USAGE_ATTR_CPU) + "        " + std::string(USAGE_ATTR_MEM) + "     " + std::string(USAGE_ATTR_IO)  + "     ", "stat", "failed" );
   }

   if (pe_task == nullptr) {
      str = "";
   } else {
      str = lGetString(pe_task, PET_id);
   }
   os << std::format("   {}{:<12.12} ", indent ? QSTAT_INDENT2 : "", str);

   /* move status info into state info */
   tstate = lGetUlong(ja_task, JAT_state);
   if (tstatus == JRUNNING) {
      tstate |= JRUNNING;
      tstate &= ~JTRANSFERING;
   } else if (tstatus == JTRANSFERING) {
      tstate |= JTRANSFERING;
      tstate &= ~JRUNNING;
   } else if (tstatus == JFINISHED) {
      tstate |= JEXITING;
      tstate &= ~(JRUNNING | JTRANSFERING);
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
   os << std::format("{:<5.5} ", task_state_string);

   {
      const lListElem *up;

      /* scaled cpu usage */
      if (!(up = lGetElemStr(scaled_usage_list, UA_name, USAGE_ATTR_CPU))) {
         os << std::format("{:<10.10} ", task_running ? "NA" : "");
      } else {
         dstring resource_string = DSTRING_INIT;

         double_print_time_to_dstring(lGetDouble(up, UA_value), &resource_string, true);
         os << sge_dstring_get_string(&resource_string) << " ";
         sge_dstring_free(&resource_string);
      }

      /* scaled mem usage */
      if (!(up = lGetElemStr(scaled_usage_list, UA_name, USAGE_ATTR_MEM))) {
         os << std::format("{:<7.7} ", task_running ? "NA" : "");
      } else {
         os << std::format("{:<5.5f} ", lGetDouble(up, UA_value));
      }

      /* scaled io usage */
      if (!(up = lGetElemStr(scaled_usage_list, UA_name, USAGE_ATTR_IO))) {
         os << std::format("{:<7.7} ", task_running ? "NA" : "");
      } else {
         os << std::format("{:<5.5f} ", lGetDouble(up, UA_value));
      }
   }

   if (tstatus == JFINISHED) {
      ep = lGetElemStr(usage_list, UA_name, "exit_status");
      os << std::format("{:<4d}", ep ? (int)lGetDouble(ep, UA_value) : 0);
   }

   putchar('\n');

   DRETURN(0);
}

/*************************************************************/
/* rel CE_Type List */
static void
sge_show_ce_type_list_line_by_line(std::ostream &os, const char *label, const char *indent, const lList *rel,
                                   bool display_resource_contribution, const lList *centry_list, int slots) {
   DENTER(TOP_LAYER);

   os << label;
   show_ce_type_list(os, rel, indent, "\n", display_resource_contribution, centry_list, slots);
   os << std::endl;

   DRETURN_VOID;
}

static int sge_print_job(std::ostream &os, lListElem *job, lListElem *jatep, lListElem *qep, int print_jobid, const char *master,
                         dstring *dyn_task_str, u_long32 full_listing, int slots, int slot, lList *exechost_list,
                         lList *centry_list, const lList *pe_list, const lList *acl_list, const char *indent, u_long32 group_opt,
                         int slots_per_line, /* number of slots to be printed in slots column
                                               when 0 is passed the number of requested slots printed */
                         int queue_name_length, ocs::QHostReportHandlerXML *report_handler, lList **alpp, u_long32 show, bool is_manager) {
   DENTER(TOP_LAYER);
   char state_string[8];
   static int first_time = 1;
   u_long32 jstate;
   int sge_urg, sge_pri, sge_ext, sge_time;
   const lList *ql = nullptr;
   const lListElem *qrep;
   const lListElem *gdil_ep = nullptr;
   int running;
   const char *queue_name = nullptr;
   int tsk_ext;
   u_long tickets, otickets, stickets, ftickets;
   int is_zombie_job;
   dstring ds;
   char buffer[128];
   dstring queue_name_buffer = DSTRING_INIT;
   char jobid[128];
   dstring output = DSTRING_INIT;
   bool dept_view = ((show & QHOST_DISPLAY_DEPT_VIEW) == QHOST_DISPLAY_DEPT_VIEW) ? true : false;
   bool hide_data = false;

   const char *owner = lGetString(job, JB_owner);
   hide_data = !job_is_visible(owner, is_manager, dept_view, acl_list);

   sge_dstring_init(&ds, buffer, sizeof(buffer));

   is_zombie_job = job_is_zombie_job(job);

   if (qep != nullptr) {
      queue_name = qinstance_get_name(qep, &queue_name_buffer);
   }

   sge_ext = ((full_listing & QSTAT_DISPLAY_EXTENDED) == QSTAT_DISPLAY_EXTENDED);
   tsk_ext = (full_listing & QSTAT_DISPLAY_TASKS);
   sge_urg = (full_listing & QSTAT_DISPLAY_URGENCY);
   sge_pri = (full_listing & QSTAT_DISPLAY_PRIORITY);
   sge_time = !sge_ext;
   sge_time = sge_time | tsk_ext | sge_urg | sge_pri;

   if (!report_handler) {
      if (first_time) {
         first_time = 0;
         if (!(full_listing & QSTAT_DISPLAY_FULL)) {
            int line_length = queue_name_length - 10 + 1;
            char *seperator = sge_malloc(line_length);
            const char *part1 = "%s%-10.10s %s %s%s%s%s%s %-10.10s %-12.12s %s%-5.5s %s%s%s%s%s%s%s%s%s%-";
            const char *part3 = ".";
            const char *part5 = "s %s %s%s%s%s%s%s";
            size_t part6_size = strlen(part1) + strlen(part3) + strlen(part5) + 256;
            char *part6 = sge_malloc(part6_size);
            {
               int i;
               for (i = 0; i < line_length; i++) {
                  seperator[i] = '-';
               }
            }
            seperator[line_length - 1] = '\0';
            snprintf(part6, part6_size, "%s%d%s%d%s", part1, queue_name_length, part3, queue_name_length, part5);

            sge_dstring_sprintf_append(&output, part6, indent, "job-ID", "prior ", (sge_pri || sge_urg) ? " nurg   " : "", sge_pri ? " npprior" : "",
                   (sge_pri || sge_ext) ? " ntckts " : "", sge_urg ? " urg      rrcontr  wtcontr  dlcontr " : "",
                   sge_pri ? "  ppri" : "", "name", "user", sge_ext ? "project          department " : "", "state",
                   sge_time ? "submit/start at     " : "", sge_urg ? " deadline           " : "",
                   sge_ext ? USAGE_ATTR_CPU "        " USAGE_ATTR_MEM "     " USAGE_ATTR_IO "      " : "",
                   sge_ext ? "tckts " : "", sge_ext ? "ovrts " : "", sge_ext ? "otckt " : "", sge_ext ? "ftckt " : "",
                   sge_ext ? "stckt " : "", sge_ext ? "share " : "", "queue",
                   (group_opt & GROUP_NO_PETASK_GROUPS) ? "master" : "slots", "ja-task-ID ", tsk_ext ? "task-ID " : "",
                   tsk_ext ? "state " : "",
                   tsk_ext ? USAGE_ATTR_CPU "        " USAGE_ATTR_MEM "     " USAGE_ATTR_IO "      " : "",
                   tsk_ext ? "stat " : "", tsk_ext ? "failed " : "");

            sge_dstring_sprintf_append(&output, "\n%s%s%s%s%s%s%s%s\n", indent, jhul1, seperator, (group_opt & GROUP_NO_PETASK_GROUPS) ? jhul2 : "",
                   sge_ext ? jhul3 : "", tsk_ext ? jhul4 : "", sge_urg ? jhul5 : "", sge_pri ? jhul6 : "");

            sge_free(&part6);
            sge_free(&seperator);
         }
      }

      sge_dstring_sprintf_append(&output, "%s", indent);

      /* job number / ja task id */
      if (print_jobid) {
         if (hide_data) {
            // maximum job id is U_LONG32_MAX = 4294967295 = 10 digits
            sge_dstring_sprintf_append(&output, "%10s ", "*");
         } else {
            sge_dstring_sprintf_append(&output, "%10" sge_fuu32 " ", lGetUlong(job, JB_job_number));
         }
      } else {
         sge_dstring_sprintf_append(&output, "           ");
      }
   } else {
      snprintf(jobid, sizeof(jobid) - 1, sge_u32, lGetUlong(job, JB_job_number));
      report_handler->job_start(os, jobid);
   }

   /* per job priority information */
   {
      if (report_handler) {
         report_handler->job_value(os, jobid, "priority", lGetDouble(jatep, JAT_prio));
      } else {
         if (print_jobid) {
            if (hide_data) {
               sge_dstring_sprintf_append(&output, "%7s ", "*");
            } else {
               sge_dstring_sprintf_append(&output, "%7.5f ", lGetDouble(jatep, JAT_prio)); /* nprio 0.0 - 1.0 */
            }
         } else {
            sge_dstring_sprintf_append(&output, "        ");
         }
      }

      if (sge_pri || sge_urg) {
         if (print_jobid) {
            if (hide_data) {
               sge_dstring_sprintf_append(&output, "%7s ", "*");
            } else {
               sge_dstring_sprintf_append(&output, "%7.5f ", lGetDouble(job, JB_nurg)); /* nurg 0.0 - 1.0 */
            }
         } else {
            sge_dstring_sprintf_append(&output, "        ");
         }
      }

      if (sge_pri) {
         if (print_jobid) {
            if (hide_data) {
               sge_dstring_sprintf_append(&output, "%7s ", "*");
            } else {
               sge_dstring_sprintf_append(&output, "%7.5f ", lGetDouble(job, JB_nppri)); /* nppri 0.0 - 1.0 */
            }
         } else {
            sge_dstring_sprintf_append(&output, "        ");
         }
      }

      if (sge_pri || sge_ext) {
         if (print_jobid) {
            if (hide_data) {
               sge_dstring_sprintf_append(&output, "%7s ", "*");
            } else {
               sge_dstring_sprintf_append(&output, "%7.5f ", lGetDouble(jatep, JAT_ntix)); /* ntix 0.0 - 1.0 */
            }
         } else {
            sge_dstring_sprintf_append(&output, "        ");
         }
      }

      if (sge_urg) {
         if (print_jobid) {
            OPTI_PRINT8(output, hide_data, lGetDouble(job, JB_urg));
            OPTI_PRINT8(output, hide_data, lGetDouble(job, JB_rrcontr));
            OPTI_PRINT8(output, hide_data, lGetDouble(job, JB_wtcontr));
            OPTI_PRINT8(output, hide_data, lGetDouble(job, JB_dlcontr));
         } else {
            sge_dstring_sprintf_append(&output, "                                    ");
         }
      }

      if (sge_pri) {
         if (print_jobid) {
            if (hide_data) {
               sge_dstring_sprintf_append(&output, "%5s ", "*");
            } else {
               sge_dstring_sprintf_append(&output, "%5d ", ((int)lGetUlong(job, JB_priority)) - BASE_PRIORITY);
            }
         } else {
            sge_dstring_sprintf_append(&output, "                  ");
         }
      }
   }

   if (report_handler) {
      report_handler->job_value(os, jobid, "qinstance_name", queue_name);
      report_handler->job_value(os, jobid, "job_name", lGetString(job, JB_job_name));
      report_handler->job_value(os, jobid, "job_owner", lGetString(job, JB_owner));
   } else {
      if (print_jobid) {
         if (hide_data) { /* job name */
            sge_dstring_sprintf_append(&output, "%-10s ", "*");
            sge_dstring_sprintf_append(&output, "%-12s ", "*");
         } else {
            sge_dstring_sprintf_append(&output, "%-10.10s ", lGetString(job, JB_job_name));
            sge_dstring_sprintf_append(&output, "%-12.12s ", lGetString(job, JB_owner));
         }
      } else {
         sge_dstring_sprintf_append(&output, "                        ");
      }
   }

   if (sge_ext) {
      const char *s;

      if (print_jobid) {
         if (hide_data) {
            sge_dstring_sprintf_append(&output, "%-16s ", "*");
            sge_dstring_sprintf_append(&output, "%-10s ", "*");
         } else {
            sge_dstring_sprintf_append(&output, "%-16.16s ", (s = lGetString(job, JB_project)) ? s : "NA");
            sge_dstring_sprintf_append(&output, "%-10.10s ", (s = lGetString(job, JB_department)) ? s : "NA");
         }
      } else {
         sge_dstring_sprintf_append(&output, "                            ");
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

   if (report_handler) {
      job_get_state_string(state_string, jstate);
      report_handler->job_value(os, jobid, "job_state", state_string);
   } else {
      if (print_jobid) {
         if (hide_data) {
            sge_dstring_sprintf_append(&output, "%-5s ", "*");
         } else {
            /* write states into string */
            job_get_state_string(state_string, jstate);
            sge_dstring_sprintf_append(&output, "%-5.5s ", state_string);
         }
      } else {
         sge_dstring_sprintf_append(&output, "      ");
      }
   }

   if (sge_time) {
      if (report_handler != nullptr) {
         u_long64 jat_start_time = lGetUlong64(jatep, JAT_start_time);
         if (jat_start_time == 0) {
            report_handler->job_value(os, jobid, "submit_time", lGetUlong64(job, JB_submission_time));
         } else {
            report_handler->job_value(os, jobid, "start_time", jat_start_time);
         }
      } else {
         if (print_jobid) {
            if (hide_data) {
               sge_dstring_sprintf_append(&output, "%s ", "*");
            } else {
               /* start/submit time */
               if (lGetUlong64(jatep, JAT_start_time) == 0) {
                  sge_dstring_sprintf_append(&output, "%s ", sge_ctime64_short(lGetUlong64(job, JB_submission_time), &ds));
               } else {
                  sge_dstring_sprintf_append(&output, "%s ", sge_ctime64_short(lGetUlong64(jatep, JAT_start_time), &ds));
               }
            }
         } else {
            sge_dstring_sprintf_append(&output, "                    ");
         }
      }
   }

   /* is job logically running */
   running = lGetUlong(jatep, JAT_status) == JRUNNING || lGetUlong(jatep, JAT_status) == JTRANSFERING;

   /* deadline time */
   if (sge_urg) {
      if (print_jobid) {
         if (lGetUlong64(job, JB_deadline) == 0) {
            sge_dstring_sprintf_append(&output, "                    ");
         } else {
            if (hide_data) {
               sge_dstring_sprintf_append(&output, "%s ", "*");
            } else {
               sge_dstring_sprintf_append(&output, "%s ", sge_ctime64_short(lGetUlong64(job, JB_deadline), &ds));
            }
         }
      } else {
         sge_dstring_sprintf_append(&output, "                    ");
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
      if (!(up = lGetElemStr(job_usage_list, UA_name, USAGE_ATTR_CPU))) {
         if (hide_data) {
            sge_dstring_sprintf_append(&output, "%-10s ", "*");
         } else {
            sge_dstring_sprintf_append(&output, "%-10.10s ", running ? "NA" : "");
         }
      } else {
         if (hide_data) {
            sge_dstring_sprintf_append(&output, "%-10s ", "*");
         } else {
            int secs, minutes, hours, days;

            secs = lGetDouble(up, UA_value);
            days = secs / (60 * 60 * 24);
            secs -= days * (60 * 60 * 24);
            hours = secs / (60 * 60);
            secs -= hours * (60 * 60);
            minutes = secs / 60;
            secs -= minutes * 60;

            sge_dstring_sprintf_append(&output, "%d:%2.2d:%2.2d:%2.2d ", days, hours, minutes, secs);
         }
      }
      /* scaled mem usage */
      if (!(up = lGetElemStr(job_usage_list, UA_name, USAGE_ATTR_MEM))) {
         sge_dstring_sprintf_append(&output, "%-7.7s ", running ? "NA" : "");
      } else {
         sge_dstring_sprintf_append(&output, "%-5.5f ", lGetDouble(up, UA_value));
      }

      /* scaled io usage */
      if (!(up = lGetElemStr(job_usage_list, UA_name, USAGE_ATTR_IO))) {
         if (hide_data) {
            sge_dstring_sprintf_append(&output, "%-7s ", "*");
         } else {
            sge_dstring_sprintf_append(&output, "%-7.7s ", running ? "NA" : "");
         }
      } else {
         if (hide_data) {
            sge_dstring_sprintf_append(&output, "%-5s ", "*");
         } else {
            sge_dstring_sprintf_append(&output, "%-5.5f ", lGetDouble(up, UA_value));
         }
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
      if (hide_data) {
            sge_dstring_sprintf_append(&output, "    * ");
            sge_dstring_sprintf_append(&output, "    * ");
            sge_dstring_sprintf_append(&output, "    * ");
            sge_dstring_sprintf_append(&output, "    * ");
            sge_dstring_sprintf_append(&output, "    * ");
            sge_dstring_sprintf_append(&output, "    * ");
      } else {
         if (is_zombie_job) {
            sge_dstring_sprintf_append(&output, "   NA ");
            sge_dstring_sprintf_append(&output, "   NA ");
            sge_dstring_sprintf_append(&output, "   NA ");
            sge_dstring_sprintf_append(&output, "   NA ");
            sge_dstring_sprintf_append(&output, "   NA ");
            sge_dstring_sprintf_append(&output, "   NA ");
         } else {
            if (sge_ext || lGetList(jatep, JAT_granted_destin_identifier_list)) {
               sge_dstring_sprintf_append(&output, "%5d ", (int)tickets);
               sge_dstring_sprintf_append(&output, "%5d ", (int)lGetUlong(job, JB_override_tickets));
               sge_dstring_sprintf_append(&output, "%5d ", (int)otickets);
               sge_dstring_sprintf_append(&output, "%5d ", (int)ftickets);
               sge_dstring_sprintf_append(&output, "%5d ", (int)stickets);
               sge_dstring_sprintf_append(&output, "%-5.2f ", lGetDouble(jatep, JAT_share));
            } else {
               sge_dstring_sprintf_append(&output, "                                          ");
            }
         }
      }
   }

   /* if not full listing we need the queue's name in each line */
   if (!(full_listing & QSTAT_DISPLAY_FULL)) {
      if (report_handler) {
         report_handler->job_value(os, jobid, "queue_name", queue_name);
      } else {
         char temp[20];
         snprintf(temp, sizeof(temp), "%%-%d.%ds ", queue_name_length, queue_name_length);
         if (hide_data) {
            sge_dstring_sprintf_append(&output, temp, "*");
         } else {
            sge_dstring_sprintf_append(&output, temp, queue_name ? queue_name : "");
         }
      }
   }

   if ((group_opt & GROUP_NO_PETASK_GROUPS)) {
      /* MASTER/SLAVE information needed only to show parallel job distribution */
      if (report_handler) {
         report_handler->job_value(os, jobid, "pe_master", master);
      } else {
         if (master) {
            if (hide_data) {
               sge_dstring_sprintf_append(&output, "%-7s", "*");
            } else {
               sge_dstring_sprintf_append(&output, "%-7.6s", master);
            }
         } else {
            sge_dstring_sprintf_append(&output, "       ");
         }
      }
   } else {
      /* job slots requested/granted */
      if (!slots_per_line) {
         slots_per_line = sge_job_slot_request(job, pe_list);
      }
      if (hide_data) {
         sge_dstring_sprintf_append(&output, "%5s ", "*");
      } else {
         sge_dstring_sprintf_append(&output, "%5d ", slots_per_line);
      }
   }

   if (report_handler) {
      const char *taskid = sge_dstring_get_string(dyn_task_str);
      if (job_is_array(job)) {
         report_handler->job_value(os, jobid, "taskid", taskid);
      }
   } else {
      if (sge_dstring_get_string(dyn_task_str) && job_is_array(job)) {
         if (hide_data) {
            sge_dstring_sprintf_append(&output, "%s", "*");
         } else {
            sge_dstring_sprintf_append(&output, "%s", sge_dstring_get_string(dyn_task_str));
         }
      } else {
         sge_dstring_sprintf_append(&output, "       ");
      }
   }

   if (tsk_ext) {
      const lList *task_list = lGetList(jatep, JAT_task_list);
      const lListElem *task, *ep;
      const char *qname;
      int indent = 0;
      int subtask_ndx = 1;
      int num_spaces =
          sizeof(jhul1) - 1 + (sge_ext ? sizeof(jhul2) - 1 : 0) - ((full_listing & QSTAT_DISPLAY_FULL) ? 11 : 0);

      /* print master sub-task belonging to this queue */
      if (!slot && task_list && queue_name && ((ep = lFirst(lGetList(jatep, JAT_granted_destin_identifier_list)))) &&
          ((qname = lGetString(ep, JG_qname))) && !strcmp(qname, queue_name)) {
         if (indent++) {
            sge_dstring_sprintf_append(&output, "%*s", num_spaces, " ");
         }
         sge_print_subtask(os, job, jatep, nullptr, 0, 0);
         /* subtask_ndx++; */
      }

      /* print sub-tasks belonging to this queue */
      for_each_ep(task, task_list) {
         if (!slots || (queue_name && ((ep = lFirst(lGetList(task, PET_granted_destin_identifier_list)))) &&
                        ((qname = lGetString(ep, JG_qname))) && !strcmp(qname, queue_name) &&
                        ((subtask_ndx++ % slots) == slot))) {
            if (indent++) {
               sge_dstring_sprintf_append(&output, "%*s", num_spaces, " ");
            }
            sge_print_subtask(os, job, jatep, task, 0, 0);
         }
      }

      if (!indent) {
         sge_dstring_sprintf_append(&output, "\n");
      }

   } else {
      if (!report_handler) {
         /* print a new line */
         os << sge_dstring_get_string(&output) << std::endl;
         sge_dstring_free(&output);
      } else {
         report_handler->job_end(os);
      }
   }

   /* print additional job info if requested */
   if (print_jobid && (full_listing & QSTAT_DISPLAY_RESOURCES)) {
      os << QSTAT_INDENT << "Full jobname:     " << lGetString(job, JB_job_name) << std::endl;

      if (queue_name) {
         os << QSTAT_INDENT << "Master queue:     " << queue_name << std::endl;
      }

      if (lGetString(job, JB_pe)) {
         dstring range_string = DSTRING_INIT;

         range_list_print_to_string(lGetList(job, JB_pe_range), &range_string, true, false, false);
         os << QSTAT_INDENT << "Requested PE:     " << lGetString(job, JB_pe) << " " << sge_dstring_get_string(&range_string) << std::endl;
         sge_dstring_free(&range_string);
      }
      if (lGetString(jatep, JAT_granted_pe)) {
         const lListElem *gdil_ep;
         u_long32 pe_slots = 0;
         for_each_ep(gdil_ep, lGetList(jatep, JAT_granted_destin_identifier_list)) {
            pe_slots += lGetUlong(gdil_ep, JG_slots);
         }
         os << QSTAT_INDENT << "Granted PE:       " << lGetString(jatep, JAT_granted_pe) << " " << pe_slots << std::endl;
      }
      if (lGetString(job, JB_checkpoint_name)) {
         os << QSTAT_INDENT << "Checkpoint Env.:  " << lGetString(job, JB_checkpoint_name) << std::endl;
      }

      sge_show_ce_type_list_line_by_line(os, QSTAT_INDENT "Hard Resources:   ", QSTAT_INDENT2,
                                         job_get_hard_resource_list(job), true, centry_list,
                                         sge_job_slot_request(job, pe_list));

      /* display default requests if necessary */
      {
         lList *attributes = nullptr;
         const lListElem *ce;
         const char *name;
         lListElem *hep;

         queue_complexes2scheduler(&attributes, qep, exechost_list, centry_list);
         for_each_ep(ce, attributes) {
            double dval;

            name = lGetString(ce, CE_name);
            if (!lGetUlong(ce, CE_consumable) || !strcmp(name, SGE_ATTR_SLOTS) || job_get_request(job, name)) {
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
                (qep && (hep = host_list_locate(exechost_list, lGetHost(qep, QU_qhostname))) &&
                 lGetSubStr(hep, CE_name, name, EH_consumable_config_list)) ||
                ((hep = host_list_locate(exechost_list, SGE_GLOBAL_NAME)) &&
                 lGetSubStr(hep, CE_name, name, EH_consumable_config_list))) {
               os << QSTAT_INDENT << name << "=" << lGetString(ce, CE_defaultval) << " (default)" << std::endl;
            }
         }
         lFreeList(&attributes);
      }

      sge_show_ce_type_list_line_by_line(os, QSTAT_INDENT "Soft Resources:   ", QSTAT_INDENT2,
                                         job_get_soft_resource_list(job), false, nullptr, 0);

      ql = job_get_hard_queue_list(job);
      if (ql) {
         os << QSTAT_INDENT << "Hard requested queues: ";
         for_each_ep(qrep, ql) {
            os << lGetString(qrep, QR_name) << (lNext(qrep) ? ", " : "\n");
         }
      }

      ql = job_get_soft_queue_list(job);
      if (ql) {
         os << QSTAT_INDENT << "Soft requested queues: ";
         for_each_ep(qrep, ql) {
            os << lGetString(qrep, QR_name) << (lNext(qrep) ? ", " : "\n");
         }
      }
      ql = job_get_master_hard_queue_list(job);
      if (ql) {
         os << QSTAT_INDENT << "Master task hard requested queues: ";
         for_each_ep(qrep, ql) {
            os << lGetString(qrep, QR_name) << (lNext(qrep) ? ", " : "\n");
         }
      }
      ql = lGetList(job, JB_jid_request_list);
      if (ql) {
         os << QSTAT_INDENT << "Predecessor Jobs (request): ";
         for_each_ep(qrep, ql) {
            os << lGetString(qrep, JRE_job_name) << (lNext(qrep) ? ", " : "\n");
         }
      }
      ql = lGetList(job, JB_jid_predecessor_list);
      if (ql) {
         os << QSTAT_INDENT << "Predecessor Jobs: ";
         for_each_ep(qrep, ql) {
            os << lGetUlong(qrep, JRE_job_number) << (lNext(qrep) ? ", " : "\n");
         }
      }
      ql = lGetList(job, JB_ja_ad_request_list);
      if (ql) {
         os << QSTAT_INDENT << "Predecessor Array Jobs (request): ";
         for_each_ep(qrep, ql) {
            os << lGetString(qrep, JRE_job_name) << (lNext(qrep) ? ", " : "\n");
         }
      }
      ql = lGetList(job, JB_ja_ad_predecessor_list);
      if (ql) {
         os << QSTAT_INDENT << "Predecessor Array Jobs: ";
         for_each_ep(qrep, ql) {
            os << lGetUlong(qrep, JRE_job_number) << (lNext(qrep) ? ", " : "\n");
         }
      }
   }

#undef QSTAT_INDENT
#undef QSTAT_INDENT2

   sge_dstring_free(&queue_name_buffer);

   DRETURN(1);
}
/*-------------------------------------------------------------------------*/
/* print jobs per queue                                                    */
/*-------------------------------------------------------------------------*/
int sge_print_jobs_queue(std::ostream &os, lListElem *qep, lList *job_list, const lList *pe_list, lList *user_list, lList *ehl,
                         lList *centry_list, int print_jobs_of_queue, u_long32 full_listing, const char *indent,
                         u_long32 group_opt, int queue_name_length, ocs::QHostReportHandlerXML *report_handler,
                         lList **alpp, u_long32 show, bool is_manager) {
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

   for_each_rw(jlep, job_list) {
      int master, i;

      for_each_rw(jatep, lGetList(jlep, JB_ja_tasks)) {
         u_long32 jstate = lGetUlong(jatep, JAT_state);

         if (shut_me_down) {
            DRETURN(1);
         }

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
                        sge_print_job(os, jlep, jatep, qep, print_jobid,
                                      (master && different && (i == 0)) ? "MASTER" : "SLAVE", &dyn_task_str,
                                      full_listing, slots_in_queue + slot_adjust, i, ehl, centry_list,
                                      pe_list, user_list, indent,
                                      group_opt, slots_per_line, queue_name_length, report_handler, alpp, show, is_manager);
                        already_printed = 1;
                     }
                     if (!already_printed && (full_listing & QSTAT_DISPLAY_SUSPENDED) &&
                         ((lGetUlong(jatep, JAT_state) & JSUSPENDED) ||
                          (lGetUlong(jatep, JAT_state) & JSUSPENDED_ON_THRESHOLD) ||
                          (lGetUlong(jatep, JAT_state) & JSUSPENDED_ON_SUBORDINATE) ||
                          (lGetUlong(jatep, JAT_state) & JSUSPENDED_ON_SLOTWISE_SUBORDINATE))) {
                        sge_print_job(os, jlep, jatep, qep, print_jobid,
                                      (master && different && (i == 0)) ? "MASTER" : "SLAVE", &dyn_task_str,
                                      full_listing, slots_in_queue + slot_adjust, i, ehl, centry_list, pe_list, user_list, indent,
                                      group_opt, slots_per_line, queue_name_length, report_handler, alpp, show, is_manager);
                        already_printed = 1;
                     }

                     if (!already_printed && (full_listing & QSTAT_DISPLAY_USERHOLD) &&
                         (lGetUlong(jatep, JAT_hold) & MINUS_H_TGT_USER)) {
                        sge_print_job(os, jlep, jatep, qep, print_jobid,
                                      (master && different && (i == 0)) ? "MASTER" : "SLAVE", &dyn_task_str,
                                      full_listing, slots_in_queue + slot_adjust, i, ehl, centry_list, pe_list, user_list, indent,
                                      group_opt, slots_per_line, queue_name_length, report_handler, alpp, show, is_manager);
                        already_printed = 1;
                     }

                     if (!already_printed && (full_listing & QSTAT_DISPLAY_OPERATORHOLD) &&
                         (lGetUlong(jatep, JAT_hold) & MINUS_H_TGT_OPERATOR)) {
                        sge_print_job(os, jlep, jatep, qep, print_jobid,
                                      (master && different && (i == 0)) ? "MASTER" : "SLAVE", &dyn_task_str,
                                      full_listing, slots_in_queue + slot_adjust, i, ehl, centry_list, pe_list, user_list, indent,
                                      group_opt, slots_per_line, queue_name_length, report_handler, alpp, show, is_manager);
                        already_printed = 1;
                     }

                     if (!already_printed && (full_listing & QSTAT_DISPLAY_SYSTEMHOLD) &&
                         (lGetUlong(jatep, JAT_hold) & MINUS_H_TGT_SYSTEM)) {
                        sge_print_job(os, jlep, jatep, qep, print_jobid,
                                      (master && different && (i == 0)) ? "MASTER" : "SLAVE", &dyn_task_str,
                                      full_listing, slots_in_queue + slot_adjust, i, ehl, centry_list, pe_list, user_list, indent,
                                      group_opt, slots_per_line, queue_name_length, report_handler, alpp, show, is_manager);
                        already_printed = 1;
                     }

                     if (!already_printed && (full_listing & QSTAT_DISPLAY_JOBARRAYHOLD) &&
                         (lGetUlong(jatep, JAT_hold) & MINUS_H_TGT_JA_AD)) {
                        sge_print_job(os, jlep, jatep, qep, print_jobid,
                                      (master && different && (i == 0)) ? "MASTER" : "SLAVE", &dyn_task_str,
                                      full_listing, slots_in_queue + slot_adjust, i, ehl, centry_list, pe_list, user_list,
                                      indent, group_opt, slots_per_line, queue_name_length, report_handler, alpp, show, is_manager);
                        already_printed = 1;
                     }
                  }
               }
            }
         }
      }
   }
   sge_dstring_free(&dyn_task_str);

   DRETURN(0);
}
