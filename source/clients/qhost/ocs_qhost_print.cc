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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <climits>
#include <math.h>
#include <cfloat>

#include "uti/sge_bootstrap.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"

#include "comm/commlib.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_eval_expression.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_qinstance_type.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_userset.h"

#include "sgeobj/sge_daemonize.h"
#include "gdi/sge_gdi.h"
#include "gdi/sge_gdi.h"

#include "sched/load_correction.h"
#include "sched/sge_complex_schedd.h"
#include "sched/sge_select_queue.h"

#include "basis_types.h"
#include "sge.h"
#include "sig_handlers.h"
#include "ocs_client_print.h"
#include "ocs_qhost_print.h"

#include "msg_common.h"
#include "msg_clients_common.h"


static int sge_print_queues(lList *ql, lListElem *hrl, lList *jl, lList *ul, lList *ehl, lList *cl, 
                            lList *pel, lList *acll, u_long32 show, qhost_report_handler_t *report_handler, lList **alpp, bool is_manager);
static int sge_print_resources(lList *ehl, lList *cl, lList *resl, lList *acl_list, lListElem *host, u_long32 show, qhost_report_handler_t *report_handler, lList **alpp);
static int sge_print_host(lListElem *hep, lList *centry_list, lList *acl_list, qhost_report_handler_t *report_handler, lList **alpp, u_long32 show, bool is_manager);

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
              lList *resource_list, u_long32 show, lList **alpp, qhost_report_handler_t* report_handler) {

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
#define HEAD_FORMAT_DEPT "%-23s %-13s%4s %4s %4s %4s %5s %7s %7s %7s %7s\n"
#define HEAD_FORMAT "%-23s %-13.13s%4.4s %4.4s %4.4s %4.4s %5.5s %7.7s %7.7s %7.7s %7.7s\n"
#define HEAD_FORMAT_OLD_DEPT "%-23s %-13s%4s %5s %7s %7s %7s %7s\n"
#define HEAD_FORMAT_OLD "%-23s %-13.13s%4.4s %5.5s %7.7s %7.7s %7.7s %7.7s\n"
   
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
   if (report_handler != nullptr) {
      ret = report_handler->report_started(report_handler, alpp);
      if (ret != QHOST_SUCCESS) {
         free_all_lists(&ql, &jl, &cl, &ehl, &pel, &acll);
         DRETURN(ret);
      }
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
            printf("-------------------------------------------------------------------------------");
            if (show_binding) {
               printf("---------------");
            } 
            printf("\n");
         }
      } else {
         ret = report_handler->report_host_begin(report_handler, lGetHost(ep, EH_name), alpp);
         if (ret != QHOST_SUCCESS) {
            break;
         }
      }
      sge_print_host(ep, cl, acll, report_handler, alpp, show, is_manager);
      sge_print_resources(ehl, cl, resource_list, acll, ep, show, report_handler, alpp);

      ret = sge_print_queues(ql, ep, jl, nullptr, ehl, cl, pel, acll, show, report_handler, alpp, is_manager);
      if (ret != QHOST_SUCCESS) {
         break;
      }
      
      if (report_handler != nullptr) {
         DPRINTF("report host_finished: %s\n", lGetHost(ep, EH_name));
         ret = report_handler->report_host_finished(report_handler, lGetHost(ep, EH_name), alpp);
         if (ret != QHOST_SUCCESS) {
            break;
         }
      }
   }   
   if (report_handler != nullptr) {
      ret = report_handler->report_finished(report_handler, alpp);
   }

   free_all_lists(&ql, &jl, &cl, &ehl, &pel, &acll);
   DRETURN(ret);
}

/*-------------------------------------------------------------------------*/
static int 
sge_print_host(lListElem *hep, lList *centry_list, lList *acl_list, qhost_report_handler_t *report_handler,
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
   bool ignore_fqdn = bootstrap_get_ignore_fqdn();
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
      sge_strlcpy(arch_string, sge_get_dominant_stringval(lep, &dominant, &rs), 
               sizeof(arch_string)); 
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
      sge_strlcpy(num_proc, sge_get_dominant_stringval(lep, &dominant, &rs),
               sizeof(num_proc)); 
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
      ret = report_handler->report_host_string_value(report_handler, "arch_string", arch_string, alpp);
      if( ret != QHOST_SUCCESS ) {
         DRETURN(ret);
      }
      ret = report_handler->report_host_string_value(report_handler, "num_proc", num_proc, alpp);
      if( ret != QHOST_SUCCESS ) {
         DRETURN(ret);
      }
      if (show_binding) {
         ret = report_handler->report_host_string_value(report_handler, "m_socket", socket, alpp);
         if( ret != QHOST_SUCCESS ) {
            DRETURN(ret);
         }
         ret = report_handler->report_host_string_value(report_handler, "m_core", core, alpp);
         if( ret != QHOST_SUCCESS ) {
            DRETURN(ret);
         }
         ret = report_handler->report_host_string_value(report_handler, "m_thread", thread, alpp);
         if( ret != QHOST_SUCCESS ) {
            DRETURN(ret);
         }
      }
      ret = report_handler->report_host_string_value(report_handler, "load_avg", load_avg, alpp);
      if( ret != QHOST_SUCCESS ) {
         DRETURN(ret);
      }
      ret = report_handler->report_host_string_value(report_handler, "mem_total", mem_total, alpp);
      if( ret != QHOST_SUCCESS ) {
         DRETURN(ret);
      }
      ret = report_handler->report_host_string_value(report_handler, "mem_used", mem_used, alpp);
      if( ret != QHOST_SUCCESS ) {
         DRETURN(ret);
      }
      ret = report_handler->report_host_string_value(report_handler, "swap_total", swap_total, alpp);
      if( ret != QHOST_SUCCESS ) {
         DRETURN(ret);
      }
      ret = report_handler->report_host_string_value(report_handler, "swap_used", swap_used, alpp);
      if( ret != QHOST_SUCCESS ) {
         DRETURN(ret);
      }
   } else {
      dstring output = DSTRING_INIT;

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
      printf("%s", sge_dstring_get_string(&output));
   }
   
   sge_dstring_free(&rs);
   
   DRETURN(ret);
}

/*-------------------------------------------------------------------------*/
static int sge_print_queues(
lList *qlp,
lListElem *host,
lList *jl,
lList *ul,
lList *ehl,
lList *cl,
lList *pel,
lList *acl_list,
u_long32 show,
qhost_report_handler_t *report_handler,
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
               ret = report_handler->report_queue_begin(report_handler, qname, alpp);
               if (ret != QHOST_SUCCESS) {
                  DRETURN(ret);
               }
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
                  ret = report_handler->report_queue_string_value(report_handler,
                                        qname, 
                                        "qtype_string", 
                                        sge_dstring_get_string(&type_string),
                                        alpp);
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
               snprintf(buf, sizeof(buf), sge_uu32"/%d/" sge_uu32 " ", qinstance_slots_reserved(qep),
                        qinstance_slots_used(qep), lGetUlong(qep, QU_job_slots));
               if (hide_data) {
                  sge_dstring_sprintf_append(&buffer, "%-14s", "*/*/*");
               } else {
                  sge_dstring_sprintf_append(&buffer, "%-14.14s", buf);
               }
            } else {
               ret = report_handler->report_queue_ulong_value(report_handler,
                                       qname, "slots_used",
                                       qinstance_slots_used(qep),
                                       alpp);
               if (ret != QHOST_SUCCESS ) {
                  DRETURN(ret);
               }
                                       
               ret = report_handler->report_queue_ulong_value(report_handler,
                                         qname, "slots",
                                         lGetUlong(qep, QU_job_slots),
                                         alpp);
               
               if (ret != QHOST_SUCCESS ) {
                  DRETURN(ret);
               }
               ret = report_handler->report_queue_ulong_value(report_handler,
                                       qname, "slots_resv",
                                       qinstance_slots_reserved(qep),
                                       alpp);
               if (ret != QHOST_SUCCESS ) {
                  DRETURN(ret);
               }
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
                  ret = report_handler->report_queue_string_value(report_handler,
                                            qname, 
                                            "state_string", 
                                            sge_dstring_get_string(&state_string),
                                            alpp);
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
               printf("%s\n", sge_dstring_get_string(&buffer));
               sge_dstring_free(&buffer);
            } else {
               ret = report_handler->report_queue_finished(report_handler, qname, alpp);
               if (ret != QHOST_SUCCESS) {
                  DRETURN(ret);
               }
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
            if (sge_print_jobs_queue(qep, jl, pel, ul, ehl, cl, 1,
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
lList *ehl,
lList *cl,
lList *resl,
lList *acl_list,
lListElem *host,
u_long32 show,
qhost_report_handler_t* report_handler,
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
      u_long32 type = lGetUlong(rep, CE_valtype);
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
                     sge_dstring_sprintf_append(&output, "    Host Resource(s):   ");
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

      switch (type) {
         case TYPE_HOST:   
         case TYPE_STR:   
         case TYPE_CSTR:   
         case TYPE_RESTR:
            if (!(lGetUlong(rep, CE_pj_dominant)&DOMINANT_TYPE_VALUE)) {
               dominant = lGetUlong(rep, CE_pj_dominant);
               s = lGetString(rep, CE_pj_stringval);
            } else {
               dominant = lGetUlong(rep, CE_dominant);
               s = lGetString(rep, CE_stringval);
            }
            break;
         case TYPE_TIM:
            if (!(lGetUlong(rep, CE_pj_dominant)&DOMINANT_TYPE_VALUE)) {
               double val = lGetDouble(rep, CE_pj_doubleval);

               dominant = lGetUlong(rep, CE_pj_dominant);
               double_print_time_to_dstring(val, &resource_string);
               s = sge_dstring_get_string(&resource_string);
            } else {
               double val = lGetDouble(rep, CE_doubleval);

               dominant = lGetUlong(rep, CE_dominant);
               double_print_time_to_dstring(val, &resource_string);
               s = sge_dstring_get_string(&resource_string);
            }
            break;
         case TYPE_MEM:
            if (!(lGetUlong(rep, CE_pj_dominant)&DOMINANT_TYPE_VALUE)) {
               double val = lGetDouble(rep, CE_pj_doubleval);

               dominant = lGetUlong(rep, CE_pj_dominant);
               double_print_memory_to_dstring(val, &resource_string);
               s = sge_dstring_get_string(&resource_string);
            } else {
               double val = lGetDouble(rep, CE_doubleval);

               dominant = lGetUlong(rep, CE_dominant);
               double_print_memory_to_dstring(val, &resource_string);
               s = sge_dstring_get_string(&resource_string);
            }
            break;
         default:   
            if (!(lGetUlong(rep, CE_pj_dominant)&DOMINANT_TYPE_VALUE)) {
               double val = lGetDouble(rep, CE_pj_doubleval);

               dominant = lGetUlong(rep, CE_pj_dominant);
               double_print_to_dstring(val, &resource_string);
               s = sge_dstring_get_string(&resource_string);
            } else {
               double val = lGetDouble(rep, CE_doubleval);

               dominant = lGetUlong(rep, CE_dominant);
               double_print_to_dstring(val, &resource_string);
               s = sge_dstring_get_string(&resource_string);
            }
            break;
      }
      monitor_dominance(dom, dominant); 
      switch(lGetUlong(rep, CE_valtype)) {
         case TYPE_INT:  
         case TYPE_TIM:  
         case TYPE_MEM:  
         case TYPE_BOO:  
         case TYPE_DOUBLE:  
         default:
            if (report_handler == nullptr) {
               if (hide_data) {
                  sge_dstring_sprintf_append(&output, "   *:*=*");
               } else {
                  sge_dstring_sprintf_append(&output, "   %s:%s=%s", dom, lGetString(rep, CE_name), s);
               }
            } else {
               ret = report_handler->report_resource_value(report_handler, dom, lGetString(rep, CE_name), s, alpp);
            }
            break;
      }

      if (report_handler == nullptr) {
         printf("%s\n", sge_dstring_get_string(&output));
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
   lList *mal = nullptr;
   lList *conf_l = nullptr;
   int q_id = 0, j_id = 0, ce_id, eh_id, pe_id, gc_id, acl_id;
   state_gdi_multi state{};
   const char *cell_root = bootstrap_get_cell_root();
   u_long32 progid = component_get_component_id();

   DENTER(TOP_LAYER);

   // @todo Should be combined with the other GDI requests
   bool perm_return = sge_gdi_get_permission(answer_list, is_manager, nullptr, nullptr, nullptr);
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
   
   eh_id = sge_gdi_multi(answer_list, SGE_GDI_RECORD, SGE_EH_LIST, SGE_GDI_GET,
                          nullptr, where, eh_all, &state, true);
   lFreeWhat(&eh_all);
   lFreeWhere(&where);

   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }
   
   if (show & QHOST_DISPLAY_JOBS || show & QHOST_DISPLAY_QUEUES) {
      q_all = lWhat("%T(ALL)", QU_Type);
      
      q_id = sge_gdi_multi(answer_list, SGE_GDI_RECORD, SGE_CQ_LIST, SGE_GDI_GET,
                            nullptr, nullptr, q_all, &state, true);
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
         nw = lWhere("%T(%I p= %s)", JB_Type, JB_owner, lGetString(ep, ST_name));
         if (!jw)
            jw = nw;
         else
            jw = lOrWhere(jw, nw);
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

/* printf("======================================\n"); */
/* lWriteWhereTo(jw, stdout); */

      j_id = sge_gdi_multi(answer_list, SGE_GDI_RECORD, SGE_JB_LIST, SGE_GDI_GET,
                         nullptr, jw, j_all, &state, true);
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
   ce_id = sge_gdi_multi(answer_list, SGE_GDI_RECORD, SGE_CE_LIST, SGE_GDI_GET,
                          nullptr, nullptr, ce_all, &state, true);
   lFreeWhat(&ce_all);

   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   /*
   ** pe list
   */
   pe_all = lWhat("%T(ALL)", PE_Type);
   pe_id = sge_gdi_multi(answer_list, SGE_GDI_RECORD, SGE_PE_LIST, SGE_GDI_GET,
                          nullptr, nullptr, pe_all, &state, true);
   lFreeWhat(&pe_all);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   /*
   ** user list
   */
   acl_all = lWhat("%T(ALL)", US_Type);
   acl_id = sge_gdi_multi(answer_list, SGE_GDI_RECORD, SGE_US_LIST, SGE_GDI_GET,
                          nullptr, nullptr, acl_all, &state, true);
   lFreeWhat(&acl_all);

   if (answer_list_has_error(answer_list)) {
      DRETURN(1);
   }

   /*
   ** global cluster configuration
   */
   gc_where = lWhere("%T(%I c= %s)", CONF_Type, CONF_name, SGE_GLOBAL_NAME);
   gc_what = lWhat("%T(ALL)", CONF_Type);
   
   gc_id = sge_gdi_multi(answer_list, SGE_GDI_SEND, SGE_CONF_LIST, SGE_GDI_GET,
                          nullptr, gc_where, gc_what, &state, true);
   sge_gdi_wait(&mal, &state);
   lFreeWhat(&gc_what);
   lFreeWhere(&gc_where);

   if (answer_list_has_error(answer_list)) {
      lFreeList(&mal);
      DRETURN(false);
   }

   /*
   ** handle results
   */
   /* --- exec host */
   gdi_extract_answer(answer_list, SGE_GDI_GET, SGE_EH_LIST, eh_id, mal, exechost_l);
   if (answer_list_has_error(answer_list)) {
      lFreeList(&mal);
      DRETURN(false);
   }

   /* --- queue */
   if (show & QHOST_DISPLAY_JOBS || show & QHOST_DISPLAY_QUEUES) {
      gdi_extract_answer(answer_list, SGE_GDI_GET, SGE_CQ_LIST, q_id, mal, queue_l);
      if (answer_list_has_error(answer_list)) {
         lFreeList(&mal);
         DRETURN(false);
      }
   }

   /* --- job */
   if (job_l && (show & QHOST_DISPLAY_JOBS)) {
      lListElem *ep = nullptr;
      gdi_extract_answer(answer_list, SGE_GDI_GET, SGE_JB_LIST, j_id, mal, job_l);
      if (answer_list_has_error(answer_list)) {
         lFreeList(&mal);
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
   gdi_extract_answer(answer_list, SGE_GDI_GET, SGE_CE_LIST, ce_id, mal, centry_l);
   if (answer_list_has_error(answer_list)) {
      lFreeList(&mal);
      DRETURN(false);
   }

   /* --- pe */
   gdi_extract_answer(answer_list, SGE_GDI_GET, SGE_PE_LIST, pe_id, mal, pe_l);
   if (answer_list_has_error(answer_list)) {
      lFreeList(&mal);
      DRETURN(false);
   }

   /* --- user lists */
   gdi_extract_answer(answer_list, SGE_GDI_GET, SGE_US_LIST, acl_id, mal, acl_l);
   if (answer_list_has_error(answer_list)) {
      lFreeList(&mal);
      DRETURN(false);
   }

   /* --- apply global configuration for sge_hostcmp() scheme */
   gdi_extract_answer(answer_list, SGE_GDI_GET, SGE_CONF_LIST, gc_id, mal, &conf_l);
   if (answer_list_has_error(answer_list)) {
      lFreeList(&mal);
      DRETURN(false);
   }
   if (lFirst(conf_l)) {
      lListElem *local = nullptr;
      merge_configuration(nullptr, progid, cell_root, lFirstRW(conf_l), local, nullptr);
   }
   
   lFreeList(&conf_l);
   lFreeList(&mal);
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
