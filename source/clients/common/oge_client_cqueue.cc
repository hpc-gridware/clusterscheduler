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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>

#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/cull/sge_resource_utilization_RUE_L.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_qinstance_type.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_userprj.h"

#include "sched/sge_complex_schedd.h"
#include "sched/sge_resource_utilization.h"
#include "sched/sge_select_queue.h"
#include "sched/valid_queue_user.h"

#include "msg_clients_common.h"
#include "oge_client_cqueue.h"
#include "oge_client_print.h"
#include "uti/sge.h"

bool cqueue_calculate_summary(const lListElem *cqueue, const lList *exechost_list, const lList *centry_list,
                              double *load, bool *is_load_available, u_long32 *used, u_long32 *resv, u_long32 *total,
                              u_long32 *suspend_manual, u_long32 *suspend_threshold, u_long32 *suspend_on_subordinate,
                              u_long32 *suspend_calendar, u_long32 *unknown, u_long32 *load_alarm,
                              u_long32 *disabled_manual, u_long32 *disabled_calendar, u_long32 *ambiguous,
                              u_long32 *orphaned, u_long32 *error, u_long32 *available, u_long32 *temp_disabled,
                              u_long32 *manual_intervention) {
   bool ret = true;

   DENTER(TOP_LAYER);
   if (cqueue != nullptr) {
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
      const lListElem *qinstance = nullptr;
      double host_load_avg = 0.0;
      u_long32 load_slots = 0;
      u_long32 used_available = 0;
      u_long32 used_slots = 0;
      u_long32 resv_slots = 0;

      *load = 0.0;
      *is_load_available = false;
      *used = *total = *resv = 0;
      *available = *temp_disabled = *manual_intervention = 0;
      *suspend_manual = *suspend_threshold = *suspend_on_subordinate = 0;
      *suspend_calendar = *unknown = *load_alarm = 0;
      *disabled_manual = *disabled_calendar = *ambiguous = 0;
      *orphaned = *error = 0;
      for_each_ep(qinstance, qinstance_list) {
         u_long32 slots = lGetUlong(qinstance, QU_job_slots);
         bool has_value_from_object;

         used_slots = qinstance_slots_used(qinstance);
         resv_slots = qinstance_slots_reserved_now(qinstance);
         (*used) += used_slots;
         (*resv) += resv_slots;
         (*total) += slots;

         if (!sge_get_double_qattr(&host_load_avg, LOAD_ATTR_NP_LOAD_AVG, qinstance, exechost_list, centry_list,
                                   &has_value_from_object)) {
            if (has_value_from_object) {
               load_slots += slots;
               *load += host_load_avg * slots;
            }
         }

         /*
          * manual_intervention: cdsuE
          * temp_disabled: aoACDS
          */
         if (qinstance_state_is_manual_suspended(qinstance) || qinstance_state_is_unknown(qinstance) ||
             qinstance_state_is_manual_disabled(qinstance) || qinstance_state_is_ambiguous(qinstance) ||
             qinstance_state_is_error(qinstance)) {
            *manual_intervention += slots;
         } else if (qinstance_state_is_alarm(qinstance) || qinstance_state_is_cal_disabled(qinstance) ||
                    qinstance_state_is_orphaned(qinstance) || qinstance_state_is_susp_on_sub(qinstance) ||
                    qinstance_state_is_cal_suspended(qinstance) || qinstance_state_is_suspend_alarm(qinstance)) {
            *temp_disabled += slots;
         } else {
            *available += slots;
            used_available += used_slots;
         }
         if (qinstance_state_is_unknown(qinstance)) {
            *unknown += slots;
         }
         if (qinstance_state_is_alarm(qinstance)) {
            *load_alarm += slots;
         }
         if (qinstance_state_is_manual_disabled(qinstance)) {
            *disabled_manual += slots;
         }
         if (qinstance_state_is_cal_disabled(qinstance)) {
            *disabled_calendar += slots;
         }
         if (qinstance_state_is_ambiguous(qinstance)) {
            *ambiguous += slots;
         }
         if (qinstance_state_is_orphaned(qinstance)) {
            *orphaned += slots;
         }
         if (qinstance_state_is_manual_suspended(qinstance)) {
            *suspend_manual += slots;
         }
         if (qinstance_state_is_susp_on_sub(qinstance)) {
            *suspend_on_subordinate += slots;
         }
         if (qinstance_state_is_cal_suspended(qinstance)) {
            *suspend_calendar += slots;
         }
         if (qinstance_state_is_suspend_alarm(qinstance)) {
            *suspend_threshold += slots;
         }
         if (qinstance_state_is_error(qinstance)) {
            *error += slots;
         }
      }
      if (load_slots > 0) {
         *is_load_available = true;
         *load /= load_slots;
      }
      *available -= used_available;
   }
   DRETURN(ret);
}

int select_by_qref_list(lList *cqueue_list, const lList *hgrp_list, const lList *qref_list) {
   int ret = 0;
   lList *queueref_list = nullptr;

   DENTER(TOP_LAYER);

   /*
    * Resolve queue pattern
    */
   {
      lList *tmp_list = nullptr;
      bool found_something = true;
      queueref_list = lCopyList("", qref_list);

      qref_list_resolve(queueref_list, nullptr, &tmp_list, &found_something, cqueue_list, hgrp_list, true, true);
      if (!found_something) {
         lFreeList(&queueref_list);
         DRETURN(-1);
      }
      lFreeList(&queueref_list);
      queueref_list = tmp_list;
      tmp_list = nullptr;
   }

   if (cqueue_list != nullptr && queueref_list != nullptr) {
      const lListElem *cqueue = nullptr;
      const lListElem *qref = nullptr;

      for_each_ep(qref, queueref_list) {
         dstring cqueue_buffer = DSTRING_INIT;
         dstring hostname_buffer = DSTRING_INIT;
         const char *full_name = nullptr;

         full_name = lGetString(qref, QR_name);

         if (cqueue_name_split(full_name, &cqueue_buffer, &hostname_buffer, nullptr, nullptr)) {
            const char *cqueue_name = sge_dstring_get_string(&cqueue_buffer);
            const char *hostname = sge_dstring_get_string(&hostname_buffer);
            const lListElem *cqueue = lGetElemStr(cqueue_list, CQ_name, cqueue_name);
            const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
            lListElem *qinstance = lGetElemHostRW(qinstance_list, QU_qhostname, hostname);

            u_long32 tag = lGetUlong(qinstance, QU_tag);
            lSetUlong(qinstance, QU_tag, tag | TAG_SELECT_IT);
         }

         sge_dstring_free(&cqueue_buffer);
         sge_dstring_free(&hostname_buffer);
      }

      for_each_ep(cqueue, cqueue_list) {
         const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
         lListElem *qinstance = nullptr;

         for_each_rw(qinstance, qinstance_list) {
            u_long32 tag = lGetUlong(qinstance, QU_tag);
            bool selected = ((tag & TAG_SELECT_IT) != 0) ? true : false;

            if (!selected) {
               tag &= ~(TAG_SELECT_IT | TAG_SHOW_IT);
               lSetUlong(qinstance, QU_tag, tag);
            } else {
               ret++;
            }
         }
      }
   }

   lFreeList(&queueref_list);
   DRETURN(ret);
}

/*
   untag all queues not selected by a -pe

   returns
      0 ok
      -1 error

*/
int select_by_pe_list(lList *queue_list, lList *peref_list, /* ST_Type */
                      lList *pe_list) {
   int nqueues = 0;
   lList *pe_selected = nullptr;
   const lListElem *pe;
   lListElem *qep;
   const lListElem *cqueue;

   DENTER(TOP_LAYER);

   /*
    * iterate through peref_list and build up a new pe_list
    * containing only those pe's referenced in peref_list
    */
   for_each_ep(pe, peref_list) {
      lListElem *ref_pe;  /* PE_Type */
      lListElem *copy_pe; /* PE_Type */

      ref_pe = pe_list_locate(pe_list, lGetString(pe, ST_name));
      copy_pe = lCopyElem(ref_pe);
      if (pe_selected == nullptr) {
         const lDescr *descriptor = lGetElemDescr(ref_pe);

         pe_selected = lCreateList("", descriptor);
      }
      lAppendElem(pe_selected, copy_pe);
   }
   if (lGetNumberOfElem(pe_selected) == 0) {
      fprintf(stderr, "%s\n", MSG_PE_NOSUCHPARALLELENVIRONMENT);
      return -1;
   }

   /*
    * untag all non-parallel queues and queues not referenced
    * by a pe in the selected pe list entry of a queue
    */
   for_each_ep(cqueue, queue_list) {
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);

      for_each_rw(qep, qinstance_list) {
         const lListElem *found = nullptr;

         if (!qinstance_is_parallel_queue(qep)) {
            lSetUlong(qep, QU_tag, 0);
            continue;
         }
         for_each_ep(pe, pe_selected) {
            const char *pe_name = lGetString(pe, PE_name);

            found = lGetSubStr(qep, ST_name, pe_name, QU_pe_list);
            if (found != nullptr) {
               break;
            }
         }
         if (found == nullptr) {
            lSetUlong(qep, QU_tag, 0);
         } else {
            nqueues++;
         }
      }
   }

   if (pe_selected != nullptr) {
      lFreeList(&pe_selected);
   }
   DRETURN(nqueues);
}

/*
   untag all queues not selected by a -pe

   returns
      0 ok
      -1 error

*/
int select_by_queue_user_list(lList *exechost_list, lList *cqueue_list, lList *queue_user_list, lList *acl_list,
                              lList *project_list) {
   int nqueues = 0;
   const lListElem *qu = nullptr;
   lListElem *qep = nullptr;
   const lListElem *cqueue = nullptr;
   lListElem *ehep = nullptr;
   const lList *h_acl = nullptr;
   const lList *h_xacl = nullptr;
   const lList *global_acl = nullptr;
   const lList *global_xacl = nullptr;
   lList *config_acl = nullptr;
   lList *config_xacl = nullptr;
   const lList *prj = nullptr;
   const lList *xprj = nullptr;
   const lList *h_prj = nullptr;
   const lList *h_xprj = nullptr;
   const lList *global_prj = nullptr;
   const lList *global_xprj = nullptr;

   DENTER(TOP_LAYER);

   /* untag all queues where no of the users has access */

   ehep = host_list_locate(exechost_list, "global");
   global_acl = lGetList(ehep, EH_acl);
   global_xacl = lGetList(ehep, EH_xacl);
   global_prj = lGetList(ehep, EH_prj);
   global_xprj = lGetList(ehep, EH_xprj);

   config_acl = mconf_get_user_lists();
   config_xacl = mconf_get_xuser_lists();

   for_each_ep(cqueue, cqueue_list) {
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);

      for_each_rw(qep, qinstance_list) {
         int access = 0;
         const char *host_name = nullptr;

         prj = lGetList(qep, QU_projects);
         xprj = lGetList(qep, QU_xprojects);

         /* get exec host list element for current queue
            and its access lists */
         host_name = lGetHost(qep, QU_qhostname);
         ehep = host_list_locate(exechost_list, host_name);
         if (ehep != nullptr) {
            h_acl = lGetList(ehep, EH_acl);
            h_xacl = lGetList(ehep, EH_xacl);
            h_prj = lGetList(ehep, EH_prj);
            h_xprj = lGetList(ehep, EH_xprj);
         }

         for_each_ep(qu, queue_user_list) {
            const lListElem *pep = nullptr;
            int q_access = 0;
            int h_access = 0;
            int gh_access = 0;
            int conf_access = 0;

            const char *name = lGetString(qu, ST_name);
            if (name == nullptr)
               continue;

            DPRINTF("-----> checking queue user: %s\n", name);

            DPRINTF("testing queue access lists\n");
            q_access = (name[0] == '@') ? sge_has_access(nullptr, &name[1], qep, acl_list)
                                        : sge_has_access(name, nullptr, qep, acl_list);
            if (!q_access) {
               DPRINTF("no access\n");
            } else {
               DPRINTF("ok\n");
            }
            if (project_list != nullptr) {
               DPRINTF("testing queue projects lists\n");
               for_each_ep(pep, prj) {
                  const char *prj_name;
                  lListElem *prj;
                  if ((prj_name = lGetString(pep, PR_name)) != nullptr) {
                     if ((prj = prj_list_locate(project_list, prj_name)) != nullptr) {
                        q_access &= (name[0] == '@') ? sge_has_access_(nullptr, &name[1], lGetList(prj, PR_acl),
                                                                       lGetList(prj, PR_xacl), acl_list)
                                                     : sge_has_access_(name, nullptr, lGetList(prj, PR_acl),
                                                                       lGetList(prj, PR_xacl), acl_list);
                     } else {
                        DPRINTF("no reference object for project %s\n", prj_name);
                     }
                  }
               }
               for_each_ep(pep, xprj) {
                  const char *prj_name;
                  lListElem *prj;
                  if ((prj_name = lGetString(pep, PR_name)) != nullptr) {
                     if ((prj = prj_list_locate(project_list, prj_name)) != nullptr) {
                        q_access &= (name[0] == '@') ? !sge_has_access_(nullptr, &name[1], lGetList(prj, PR_acl),
                                                                        lGetList(prj, PR_xacl), acl_list)
                                                     : !sge_has_access_(name, nullptr, lGetList(prj, PR_acl),
                                                                        lGetList(prj, PR_xacl), acl_list);
                     } else {
                        DPRINTF("no reference object for project %s\n", prj_name);
                     }
                  }
               }
               if (!q_access) {
                  DPRINTF("no access\n");
               } else {
                  DPRINTF("ok\n");
               }
            }

            DPRINTF("testing host access lists\n");
            h_access = (name[0] == '@') ? sge_has_access_(nullptr, &name[1], h_acl, h_xacl, acl_list)
                                        : sge_has_access_(name, nullptr, h_acl, h_xacl, acl_list);
            if (!h_access) {
               DPRINTF("no access\n");
            } else {
               DPRINTF("ok\n");
            }
            if (project_list != nullptr) {
               DPRINTF("testing host projects lists\n");
               for_each_ep(pep, h_prj) {
                  const char *prj_name;
                  lListElem *prj;
                  if ((prj_name = lGetString(pep, PR_name)) != nullptr) {
                     if ((prj = prj_list_locate(project_list, prj_name)) != nullptr) {
                        q_access &= (name[0] == '@') ? sge_has_access_(nullptr, &name[1], lGetList(prj, PR_acl),
                                                                       lGetList(prj, PR_xacl), acl_list)
                                                     : sge_has_access_(name, nullptr, lGetList(prj, PR_acl),
                                                                       lGetList(prj, PR_xacl), acl_list);
                     } else {
                        DPRINTF("no reference object for project %s\n", prj_name);
                     }
                  }
               }
               for_each_ep(pep, h_xprj) {
                  const char *prj_name;
                  lListElem *prj;
                  if ((prj_name = lGetString(pep, PR_name)) != nullptr) {
                     if ((prj = prj_list_locate(project_list, prj_name)) != nullptr) {
                        q_access &= (name[0] == '@') ? !sge_has_access_(nullptr, &name[1], lGetList(prj, PR_acl),
                                                                        lGetList(prj, PR_xacl), acl_list)
                                                     : !sge_has_access_(name, nullptr, lGetList(prj, PR_acl),
                                                                        lGetList(prj, PR_xacl), acl_list);
                     } else {
                        DPRINTF("no reference object for project %s\n", prj_name);
                     }
                  }
               }
               if (!q_access) {
                  DPRINTF("no access\n");
               } else {
                  DPRINTF("ok\n");
               }
            }

            DPRINTF("testing global host access lists\n");
            gh_access = (name[0] == '@') ? sge_has_access_(nullptr, &name[1], global_acl, global_xacl, acl_list)
                                         : sge_has_access_(name, nullptr, global_acl, global_xacl, acl_list);
            if (!gh_access) {
               DPRINTF("no access\n");
            } else {
               DPRINTF("ok\n");
            }
            if (project_list != nullptr) {
               DPRINTF("testing host projects lists\n");
               for_each_ep(pep, global_prj) {
                  const char *prj_name;
                  lListElem *prj;
                  if ((prj_name = lGetString(pep, PR_name)) != nullptr) {
                     if ((prj = prj_list_locate(project_list, prj_name)) != nullptr) {
                        q_access &= (name[0] == '@') ? sge_has_access_(nullptr, &name[1], lGetList(prj, PR_acl),
                                                                       lGetList(prj, PR_xacl), acl_list)
                                                     : sge_has_access_(name, nullptr, lGetList(prj, PR_acl),
                                                                       lGetList(prj, PR_xacl), acl_list);
                     } else {
                        DPRINTF("no reference object for project %s\n", prj_name);
                     }
                  }
               }
               for_each_ep(pep, global_xprj) {
                  const char *prj_name;
                  lListElem *prj;
                  if ((prj_name = lGetString(pep, PR_name)) != nullptr) {
                     if ((prj = prj_list_locate(project_list, prj_name)) != nullptr) {
                        q_access &= (name[0] == '@') ? !sge_has_access_(nullptr, &name[1], lGetList(prj, PR_acl),
                                                                        lGetList(prj, PR_xacl), acl_list)
                                                     : !sge_has_access_(name, nullptr, lGetList(prj, PR_acl),
                                                                        lGetList(prj, PR_xacl), acl_list);
                     } else {
                        DPRINTF("no reference object for project %s\n", prj_name);
                     }
                  }
               }
               if (!q_access) {
                  DPRINTF("no access\n");
               } else {
                  DPRINTF("ok\n");
               }
            }

            DPRINTF("testing cluster config access lists\n");
            conf_access = (name[0] == '@') ? sge_has_access_(nullptr, &name[1], config_acl, config_xacl, acl_list)
                                           : sge_has_access_(name, nullptr, config_acl, config_xacl, acl_list);
            if (!conf_access) {
               DPRINTF("no access\n");
            } else {
               DPRINTF("ok\n");
            }

            access = q_access && h_access && gh_access && conf_access;
            if (!access) {
               break;
            }
         }
         if (!access) {
            DPRINTF("no access for queue %s\n", lGetString(qep, QU_qname));
            lSetUlong(qep, QU_tag, 0);
         } else {
            DPRINTF("access for queue %s\n", lGetString(qep, QU_qname));
            nqueues++;
         }
      }
   }

   lFreeList(&config_acl);
   lFreeList(&config_xacl);
   DRETURN(nqueues);
}

/*
   untag all queues not in a specific state

   returns
      0 ok
      -1 error

*/
int select_by_queue_state(u_long32 queue_states, lList *exechost_list, lList *queue_list, lList *centry_list) {
   bool has_value_from_object;
   double load_avg;
   const char *load_avg_str;
   const lListElem *cqueue = nullptr;
   u_long32 interval;

   DENTER(TOP_LAYER);

   /* only show queues in the requested state */
   /* make it possible to display any load value in qstat output */
   if (!(load_avg_str = getenv("SGE_LOAD_AVG")) || !strlen(load_avg_str))
      load_avg_str = LOAD_ATTR_LOAD_AVG;

   for_each_ep(cqueue, queue_list) {
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
      lListElem *qep = nullptr;
      for_each_rw(qep, qinstance_list) {

         /* compute the load and suspend alarm */
         sge_get_double_qattr(&load_avg, load_avg_str, qep, exechost_list, centry_list, &has_value_from_object);
         if (sge_load_alarm(nullptr, 0, qep, lGetList(qep, QU_load_thresholds), exechost_list, centry_list, nullptr, true)) {
            qinstance_state_set_alarm(qep, true);
         }
         parse_ulong_val(nullptr, &interval, TYPE_TIM, lGetString(qep, QU_suspend_interval), nullptr, 0);
         if (lGetUlong(qep, QU_nsuspend) != 0 && interval != 0 &&
             sge_load_alarm(nullptr, 0, qep, lGetList(qep, QU_suspend_thresholds), exechost_list, centry_list, nullptr, false)) {
            qinstance_state_set_suspend_alarm(qep, true);
         }

         if (!qinstance_has_state(qep, queue_states)) {
            lSetUlong(qep, QU_tag, 0);
         }
      }
   }
   DRETURN(0);
}

/*
   untag all queues not covered by -l

   returns
      0  successfully untagged qinstances if necessary
     -1  error

*/
int select_by_resource_list(lList *resource_list, lList *exechost_list, lList *queue_list, lList *centry_list,
                            u_long32 empty_qs) {
   const lListElem *cqueue = nullptr;

   DENTER(TOP_LAYER);

   if (centry_list_fill_request(resource_list, nullptr, centry_list, true, true, false)) {
      /*
      ** error message gets written by centry_list_fill_request into
      ** SGE_EVENT
      */
      DRETURN(-1);
   }

   /* prepare request */
   for_each_ep(cqueue, queue_list) {
      lListElem *qep;
      bool selected;
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);

      for_each_rw(qep, qinstance_list) {
         if (empty_qs)
            sconf_set_qs_state(QS_STATE_EMPTY);

         selected = sge_select_queue(resource_list, qep, nullptr, exechost_list, centry_list, true, -1, nullptr, nullptr, nullptr);
         if (empty_qs)
            sconf_set_qs_state(QS_STATE_FULL);

         if (!selected)
            lSetUlong(qep, QU_tag, 0);
      }
   }

   DRETURN(0);
}

bool is_cqueue_selected(lList *queue_list) {
   lListElem *cqueue;
   bool a_qinstance_is_selected = false;
   bool a_cqueue_is_selected = false;

   DENTER(TOP_LAYER);

   for_each_rw(cqueue, queue_list) {
      const lListElem *qep;
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
      bool tmp_a_qinstance_is_selected = false;

      for_each_ep(qep, qinstance_list) {
         if (lGetUlong(qep, QU_tag) & TAG_SHOW_IT) {
            tmp_a_qinstance_is_selected = true;
            break;
         }
      }
      a_qinstance_is_selected = a_qinstance_is_selected | tmp_a_qinstance_is_selected;
      if (!tmp_a_qinstance_is_selected && (lGetNumberOfElem(lGetList(cqueue, CQ_qinstances)) > 0)) {
         lSetUlong(cqueue, CQ_tag, TAG_DEFAULT);
      } else {
         a_cqueue_is_selected |= true;
      }
   }

   DRETURN(a_cqueue_is_selected);
}

/****** sge_cqueue_qstat/qinstance_slots_reserved_now() ************************
 *  NAME
 *     qinstance_slots_reserved_now() -- get current reserved slots
 *
 *  SYNOPSIS
 *     int qinstance_slots_reserved_now(const lListElem *this_elem)
 *
 *  FUNCTION
 *     returns the current number of reserved slots
 *
 *  INPUTS
 *     const lListElem *this_elem - queue elem (QU_Type)
 *
 *  RESULT
 *     int - number of currently reserved slots
 *
 *  NOTES
 *     MT-NOTE: qinstance_slots_reserved_now() is MT safe
 *
 *  SEE ALSO
 *     qinstance_slots_reserved()
 *******************************************************************************/
int qinstance_slots_reserved_now(const lListElem *this_elem) {
   DENTER(TOP_LAYER);
   int ret = 0;
   const lListElem *slots = lGetSubStr(this_elem, RUE_name, SGE_ATTR_SLOTS, QU_resource_utilization);
   if (slots != nullptr) {
      u_long64 now = sge_get_gmt64();
      ret = utilization_max(slots, now, 0, false);
   }
   DRETURN(ret);
}
