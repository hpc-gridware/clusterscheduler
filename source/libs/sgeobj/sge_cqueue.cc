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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Cluster queues: the template the queue instances are derived from
 *
 * An administrator configures a cluster queue; qmaster derives one queue
 * instance per host from it. Most attributes are therefore stored as a list of
 * host or host group specific values, and #cqueue_attribute_array is the table
 * saying, for each attribute, where its list lives and where the resolved
 * value ends up in the instance.
 *
 * @see sge_cqueue.h
 */

#include <cstring>

#include "uti/ocs_Pattern.h"
#include "uti/sge_dstring.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge.h"
#include "uti/sge_stdlib.h"

#include "comm/commlib.h"

#include "cull/cull_list.h"

#include "sgeobj/parse.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_cqueue_verify.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_type.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_subordinate.h"
#include "sgeobj/sge_eval_expression.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_HR.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_SO.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_ABOOL.h"
#include "sgeobj/tom/ocs_tom_ACELIST.h"
#include "sgeobj/tom/ocs_tom_AINTER.h"
#include "sgeobj/tom/ocs_tom_AMEM.h"
#include "sgeobj/tom/ocs_tom_APRJLIST.h"
#include "sgeobj/tom/ocs_tom_AQTLIST.h"
#include "sgeobj/tom/ocs_tom_ASOLIST.h"
#include "sgeobj/tom/ocs_tom_ASTR.h"
#include "sgeobj/tom/ocs_tom_ASTRLIST.h"
#include "sgeobj/tom/ocs_tom_ATIME.h"
#include "sgeobj/tom/ocs_tom_AULNG.h"
#include "sgeobj/tom/ocs_tom_AUSRLIST.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PR.h"

#include "msg_clients_common.h"
#include "msg_common.h"

/// Debug layer the cluster queue traces are written to
#define CQUEUE_LAYER TOP_LAYER

/* *INDENT-OFF* */

/// Every cluster queue attribute, terminated by an entry whose `cqueue_attr` is `NoName`
list_attribute_struct cqueue_attribute_array[] = {
   { CQ_seq_no,                  QU_seq_no,                 AULNG_href,    AULNG_value,      NoName,     SGE_ATTR_SEQ_NO,            false,  false, nullptr},
   { CQ_nsuspend,                QU_nsuspend,               AULNG_href,    AULNG_value,      NoName,     SGE_ATTR_NSUSPEND,          false,  false, nullptr},
   { CQ_job_slots,               QU_job_slots,              AULNG_href,    AULNG_value,      NoName,     SGE_ATTR_SLOTS,             false,  false, cqueue_verify_job_slots},

   { CQ_tmpdir,                  QU_tmpdir,                 ASTR_href,     ASTR_value,       NoName,     SGE_ATTR_TMPDIR,            false,  false, nullptr},
   { CQ_shell,                   QU_shell,                  ASTR_href,     ASTR_value,       NoName,     SGE_ATTR_SHELL,             false,  true,  cqueue_verify_shell},
   { CQ_calendar,                QU_calendar,               ASTR_href,     ASTR_value,       NoName,     SGE_ATTR_CALENDAR,          false,  false, cqueue_verify_calendar},
   { CQ_priority,                QU_priority,               ASTR_href,     ASTR_value,       NoName,     SGE_ATTR_PRIORITY,          false,  true,  cqueue_verify_priority},
   { CQ_processors,              QU_processors,             ASTR_href,     ASTR_value,       NoName,     SGE_ATTR_PROCESSORS,        false,  true,  cqueue_verify_processors},
   { CQ_prolog,                  QU_prolog,                 ASTR_href,     ASTR_value,       NoName,     SGE_ATTR_PROLOG,            false,  false, nullptr},
   { CQ_epilog,                  QU_epilog,                 ASTR_href,     ASTR_value,       NoName,     SGE_ATTR_EPILOG,            false,  false, nullptr},
   { CQ_shell_start_mode,        QU_shell_start_mode,       ASTR_href,     ASTR_value,       NoName,     SGE_ATTR_SHELL_START_MODE,  false,  true,  cqueue_verify_shell_start_mode},
   { CQ_starter_method,          QU_starter_method,         ASTR_href,     ASTR_value,       NoName,     SGE_ATTR_STARTER_METHOD,    false,  false, nullptr},
   { CQ_suspend_method,          QU_suspend_method,         ASTR_href,     ASTR_value,       NoName,     SGE_ATTR_SUSPEND_METHOD,    false,  false, nullptr},
   { CQ_resume_method,           QU_resume_method,          ASTR_href,     ASTR_value,       NoName,     SGE_ATTR_RESUME_METHOD,     false,  false, nullptr},
   { CQ_terminate_method,        QU_terminate_method,       ASTR_href,     ASTR_value,       NoName,     SGE_ATTR_TERMINATE_METHOD,  false,  false, nullptr},
   { CQ_initial_state,           QU_initial_state,          ASTR_href,     ASTR_value,       NoName,     SGE_ATTR_INITIAL_STATE,     false,  true,  cqueue_verify_initial_state},
   
   { CQ_rerun,                   QU_rerun,                  ABOOL_href,    ABOOL_value,      NoName,     SGE_ATTR_RERUN,             false,  false, nullptr},

   { CQ_s_fsize,                 QU_s_fsize,                AMEM_href,     AMEM_value,       NoName,     SGE_ATTR_S_FSIZE,           false,  false, cqueue_verify_memory_value},
   { CQ_h_fsize,                 QU_h_fsize,                AMEM_href,     AMEM_value,       NoName,     SGE_ATTR_H_FSIZE,           false,  false, cqueue_verify_memory_value},
   { CQ_s_data,                  QU_s_data,                 AMEM_href,     AMEM_value,       NoName,     SGE_ATTR_S_DATA,            false,  false, cqueue_verify_memory_value},
   { CQ_h_data,                  QU_h_data,                 AMEM_href,     AMEM_value,       NoName,     SGE_ATTR_H_DATA,            false,  false, cqueue_verify_memory_value},
   { CQ_s_stack,                 QU_s_stack,                AMEM_href,     AMEM_value,       NoName,     SGE_ATTR_S_STACK,           false,  false, cqueue_verify_memory_value},
   { CQ_h_stack,                 QU_h_stack,                AMEM_href,     AMEM_value,       NoName,     SGE_ATTR_H_STACK,           false,  false, cqueue_verify_memory_value},
   { CQ_s_core,                  QU_s_core,                 AMEM_href,     AMEM_value,       NoName,     SGE_ATTR_S_CORE,            false,  false, cqueue_verify_memory_value},
   { CQ_h_core,                  QU_h_core,                 AMEM_href,     AMEM_value,       NoName,     SGE_ATTR_H_CORE,            false,  false, cqueue_verify_memory_value},
   { CQ_s_rss,                   QU_s_rss,                  AMEM_href,     AMEM_value,       NoName,     SGE_ATTR_S_RSS,             false,  false, cqueue_verify_memory_value},
   { CQ_h_rss,                   QU_h_rss,                  AMEM_href,     AMEM_value,       NoName,     SGE_ATTR_H_RSS,             false,  false, cqueue_verify_memory_value},
   { CQ_s_vmem,                  QU_s_vmem,                 AMEM_href,     AMEM_value,       NoName,     SGE_ATTR_S_VMEM,            false,  false, cqueue_verify_memory_value},
   { CQ_h_vmem,                  QU_h_vmem,                 AMEM_href,     AMEM_value,       NoName,     SGE_ATTR_H_VMEM,            false,  false, cqueue_verify_memory_value},

   { CQ_s_rt,                    QU_s_rt,                   ATIME_href,    ATIME_value,      NoName,     SGE_ATTR_S_RT,              false,  false, cqueue_verify_time_value},
   { CQ_h_rt,                    QU_h_rt,                   ATIME_href,    ATIME_value,      NoName,     SGE_ATTR_H_RT,              false,  false, cqueue_verify_time_value},
   { CQ_s_cpu,                   QU_s_cpu,                  ATIME_href,    ATIME_value,      NoName,     SGE_ATTR_S_CPU,             false,  false, cqueue_verify_time_value},
   { CQ_h_cpu,                   QU_h_cpu,                  ATIME_href,    ATIME_value,      NoName,     SGE_ATTR_H_CPU,             false,  false, cqueue_verify_time_value},

   { CQ_suspend_interval,        QU_suspend_interval,       AINTER_href,   AINTER_value,     NoName,     SGE_ATTR_SUSPEND_INTERVAL,  false,  false, nullptr},
   { CQ_min_cpu_interval,        QU_min_cpu_interval,       AINTER_href,   AINTER_value,     NoName,     SGE_ATTR_MIN_CPU_INTERVAL,  false,  false, nullptr},
   { CQ_notify,                  QU_notify,                 AINTER_href,   AINTER_value,     NoName,     SGE_ATTR_NOTIFY,            false,  false, nullptr},

   { CQ_qtype,                   QU_qtype,                  AQTLIST_href,  AQTLIST_value,    NoName,     SGE_ATTR_QTYPE,             false,  false, nullptr},

   { CQ_ckpt_list,               QU_ckpt_list,              ASTRLIST_href, ASTRLIST_value,   ST_name,    SGE_ATTR_CKPT_LIST,         false,  false, cqueue_verify_ckpt_list},
   { CQ_pe_list,                 QU_pe_list,                ASTRLIST_href, ASTRLIST_value,   ST_name,    SGE_ATTR_PE_LIST,           false,  false, cqueue_verify_pe_list},
 
   { CQ_owner_list,              QU_owner_list,             AUSRLIST_href, AUSRLIST_value,   US_name,    SGE_ATTR_OWNER_LIST,        false,  false, nullptr},
   { CQ_acl,                     QU_acl,                    AUSRLIST_href, AUSRLIST_value,   US_name,    SGE_ATTR_USER_LISTS,        false,  false, cqueue_verify_user_list},
   { CQ_xacl,                    QU_xacl,                   AUSRLIST_href, AUSRLIST_value,   US_name,    SGE_ATTR_XUSER_LISTS,       false,  false, cqueue_verify_user_list},

   { CQ_projects,                QU_projects,               APRJLIST_href, APRJLIST_value,   PR_name,    SGE_ATTR_PROJECTS,          true,   false, cqueue_verify_project_list},
   { CQ_xprojects,               QU_xprojects,              APRJLIST_href, APRJLIST_value,   PR_name,    SGE_ATTR_XPROJECTS,         true,   false, cqueue_verify_project_list},

   { CQ_consumable_config_list,  QU_consumable_config_list, ACELIST_href,  ACELIST_value,    CE_name,    SGE_ATTR_COMPLEX_VALUES,    false,  false, cqueue_verify_consumable_config_list},
   { CQ_load_thresholds,         QU_load_thresholds,        ACELIST_href,  ACELIST_value,    CE_name,    SGE_ATTR_LOAD_THRESHOLD,    false,  false, nullptr},
   { CQ_suspend_thresholds,      QU_suspend_thresholds,     ACELIST_href,  ACELIST_value,    CE_name,    SGE_ATTR_SUSPEND_THRESHOLD, false,  false, nullptr},

   { CQ_subordinate_list,        QU_subordinate_list,       ASOLIST_href,  ASOLIST_value,    SO_name,    SGE_ATTR_SUBORDINATE_LIST,  false,  false, cqueue_verify_subordinate_list},

   { NoName,                     NoName,                    NoName,        NoName,           NoName,     nullptr,                       false,  false, nullptr}
};

/* *INDENT-ON* */

/* EB: ADOC: add commets */

/**
 * @brief Get the CQ and host part of a QI name
 *
 * Splits a qinstance name into its components.
 * Examples:
 * QI-name         cqueue_name  host_domain  has_hostname  has_domain
 * ------------------------------------------------------------------
 * all.q           all.q        ""           false         false
 * `all.q@hostname`  all.q        hostname     true          false
 * `all.q@@hgrp`     all.q        `@hgrp`        false         true
 *
 * @param name CQ/QD or QI name
 * @param cqueue_name CQ part of the name
 * @param host_domain host or hostgroup or nothing
 * @param has_hostname is "host_domain" a hostname
 * @param has_domain if "host_domain" a hostgroup
 *
 * @return error state always true  - success
 */
bool cqueue_name_split(const char *name,
                       dstring *cqueue_name, dstring *host_domain,
                       bool *has_hostname, bool *has_domain) {
   DENTER(CQUEUE_LAYER);

   bool ret = true;

   if (has_hostname)
      *has_hostname = false;
   if (has_domain)
      *has_domain = false;

   if (name != nullptr && cqueue_name != nullptr && host_domain != nullptr ) {
      bool at_skiped = false;

      sge_dstring_clear(cqueue_name);
      sge_dstring_clear(host_domain);

      while (*name != '\0') {
         if (!at_skiped && *name == '@') {
            at_skiped = true;
            name++;
            if (*name == '\0') {
               ret = false;
               break;
            }
            if (*name == '@') {
               if (*(name + 1) == '\0') {
                  ret = false;
                  break;
               }
               if (has_domain)
                  *has_domain = true;
               if (has_hostname)
                  *has_hostname = false;
            } else {
               if (has_domain)
                  *has_domain = false;
               if (has_hostname)
                  *has_hostname = true;
            }
            continue; 
         }
         if (!at_skiped) {
            sge_dstring_append_char(cqueue_name, name[0]);
         } else {
            sge_dstring_append_char(host_domain, name[0]);
         }
         name++;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Returns the cluster queue part of a queue
 *
 * Returns a character pointer to a newly malloced string containing the cluster
 * queue part of a queue instance name.
 * The memory needs to be free'd by the caller
 *
 * @param queue_instance queue instance or cluster queue
 *
 * @return cluster queue name
 *
 * @note MT-NOTE: cqueue_get_name_from_qinstance() is MT safe
 */
char *cqueue_get_name_from_qinstance(const char *queue_instance) {
   char *at_sign = nullptr;
   char *cqueue = nullptr;

   if ((at_sign = strchr((char *)queue_instance, '@'))) {
      int size = at_sign - queue_instance;
      cqueue = sge_malloc(sizeof(char) * (size + 1));
      cqueue = strncpy(cqueue, queue_instance, size);
      cqueue[size] = '\0';
   } else {
      cqueue = strdup(queue_instance);
   }

   return cqueue;
}

/**
 * @brief Create a new cluster queue object
 *
 * Returns a new cluster queue object with the name "name".
 *
 * @param answer_list AN_Type list
 * @param name cluster queue name
 *
 * @return CQ_Type object or nullptr
 */
lListElem *
cqueue_create(lList **answer_list, const char *name) {
   DENTER(CQUEUE_LAYER);

   lListElem *ret = nullptr;

   if (name != nullptr) {
      ret = lCreateElem(CQ_Type);

      if (ret != nullptr) {
         ocs::tom::set<CQ_name>(ret, name);
      } else {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_MEM_MEMORYALLOCFAILED_S, __func__);
         answer_list_add(answer_list, SGE_EVENT, STATUS_EMALLOC, ANSWER_QUALITY_ERROR);
      }
   }
   DRETURN(ret);
}

/**
 * @brief The host list of a cluster queue
 *
 * CS-2677. A cluster queue does not carry a host list any more -- it owns the
 * host group `@@<queue>`, and that group's member list is the host list. This
 * is the accessor that replaces `lGetList(cqueue, CQ_hostlist)`, and the only
 * way the qmaster should reach it: the field of the same name still exists,
 * but it carries an instruction on its way in and is empty on every element
 * the master list holds.
 *
 * What is returned is the member list as written -- host names, group
 * references, and whatever else a host group may contain. Callers wanting the
 * hosts it reaches resolve it with href_list_find_all_references().
 *
 * @param cqueue CQ_Type object, may be an empty handle
 * @param master_hgroup_list HGRP_Type master list, may be an empty handle
 *
 * @return the member list (HR_Type), or an empty handle when the queue has no group yet
 *
 * @note MT-NOTE: cqueue_get_hostlist() is MT safe
 */
ocs::tom::ConstList<ocs::tom::HR> cqueue_get_hostlist(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue,
                                                      const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list) {
   if (!cqueue) {
      return nullptr;
   }
   dstring name_buffer = DSTRING_INIT;
   const char *group_name = hgroup_queue_group_name(ocs::tom::get<CQ_name>(cqueue), &name_buffer);
   const auto hgroup = hgroup_list_locate(master_hgroup_list, group_name);

   sge_dstring_free(&name_buffer);

   if (!hgroup) {
      return nullptr;
   }
   return ocs::tom::get<HGRP_host_list>(hgroup);
}

/**
 * @brief cqueue_get_hostlist() for an untyped caller
 *
 * @param cqueue CQ_Type object, checked; may be nullptr
 * @param master_hgroup_list HGRP_Type master list, checked; may be nullptr
 *
 * @return the member list (HR_Type), or nullptr when the queue has no group yet
 */
const lList *cqueue_get_hostlist(const lListElem *cqueue, const lList *master_hgroup_list) {
   return cqueue_get_hostlist(ocs::tom::ConstElement<ocs::tom::CQ>(cqueue),
                              ocs::tom::ConstList<ocs::tom::HGRP>(master_hgroup_list)).raw();
}

/**
 * @brief Put the host list onto a cluster queue, for a client that renders it
 *
 * CS-2677, the client side of cqueue_get_hostlist(). What GDI hands out is a
 * queue without a host list -- the list is the member list of the queue's host
 * group, and travels as a host group. A client that displays or edits a queue
 * configuration wants the two joined again, in the shape the configuration
 * format has always had (N-X-1), and this joins them: on the client's own
 * copy, from the host groups it fetched in the same request.
 *
 * The field it fills is the one a write travels in, so an element completed
 * here and sent back unchanged reproduces the same member list -- which is
 * what makes the round trip idempotent (N-X-3).
 *
 * @param cqueue CQ_Type object, modified in place; may be an empty handle
 * @param master_hgroup_list HGRP_Type list, as fetched from the qmaster
 *
 * @note MT-NOTE: cqueue_fill_hostlist() is MT safe
 */
void cqueue_fill_hostlist(const ocs::tom::Element<ocs::tom::CQ> cqueue,
                          const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list) {
   const auto href_list = cqueue_get_hostlist(cqueue, master_hgroup_list);

   if (cqueue) {
      // a copy named "", or nullptr for a queue without a group, as lCopyList() gave
      ocs::tom::set<CQ_hostlist>(cqueue, href_list.clone(""));
   }
}

/**
 * @brief Put the host list onto every cluster queue of a list
 *
 * @param cqueue_list CQ_Type list, modified in place; may be an empty handle
 * @param master_hgroup_list HGRP_Type list, as fetched from the qmaster
 *
 * @note MT-NOTE: cqueue_list_fill_hostlist() is MT safe
 */
void cqueue_list_fill_hostlist(const ocs::tom::List<ocs::tom::CQ> cqueue_list,
                               const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list) {
   for (const auto cqueue : cqueue_list) {
      cqueue_fill_hostlist(cqueue, master_hgroup_list);
   }
}

/**
 * @brief cqueue_list_fill_hostlist() for an untyped caller
 *
 * @param cqueue_list CQ_Type list, checked; modified in place; may be nullptr
 * @param master_hgroup_list HGRP_Type list, checked; may be nullptr
 */
void cqueue_list_fill_hostlist(lList *cqueue_list, const lList *master_hgroup_list) {
   cqueue_list_fill_hostlist(ocs::tom::List<ocs::tom::CQ>(cqueue_list),
                             ocs::tom::ConstList<ocs::tom::HGRP>(master_hgroup_list));
}

/**
 * @brief Is a host/hostgroup referenced in cqueue
 *
 * Is the given "href" (host or hostgroup referenece) used in the
 * definition of the cluster queue "this_elem"? If "only_hostlist"
 * is true then only the hostlist will be tested and all
 * parameter lists will be ignored.
 *
 * @param this_elem CQ_Type, may be an empty handle
 * @param href HR_Type, may be an empty handle
 * @param only_hostlist check only hostlist and ignore all parameter lists
 *
 * @return true if it is referenced
 */
bool cqueue_is_href_referenced(const ocs::tom::ConstElement<ocs::tom::CQ> this_elem,
                               const ocs::tom::ConstElement<ocs::tom::HR> href, const bool only_hostlist) {
   DENTER(CQUEUE_LAYER);

   bool ret = false;

   if (this_elem && href) {
      const char *href_name = ocs::tom::get<HR_name>(href);
      
      if (href_name != nullptr) {
         /*
          * Is it the host group carrying this queue's host list ...
          *
          * CS-2677, N-T-5. This used to be a lookup in the queue's own host
          * list. There is none any more: the host list is the member list of
          * "@@<queue>", and no host list names that group -- its own included.
          * The relation is carried by the name, so the name is what is asked.
          */
         dstring group_buffer = DSTRING_INIT;
         const char *own_group = hgroup_queue_group_name(ocs::tom::get<CQ_name>(this_elem),
                                                         &group_buffer);

         if (own_group != nullptr && strcmp(own_group, href_name) == 0) {
            ret = true;
         }
         sge_dstring_free(&group_buffer);

         /*
          * ... or is it contained on one of the attribute lists
          */
         if (!only_hostlist) {
            int index = 0;

            while (cqueue_attribute_array[index].cqueue_attr != NoName && !ret) {
               // the attribute and its host field are given at run time, so the untyped access stays
               const lList *attr_list = lGetList(this_elem, cqueue_attribute_array[index].cqueue_attr);
               const lListElem *attr_elem = lGetElemHost(attr_list, cqueue_attribute_array[index].href_attr,
                                                         href_name);

               if (attr_elem != nullptr) {
                  ret = true;
               }
               index++;
            }
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Is a hgroup referenced in cqueue
 *
 * Is the given "hgroup" object referenced in the cluster queue
 * "this_elem".
 *
 * @param this_elem CQ_Type, may be an empty handle
 * @param hgroup HGRP_Type, may be an empty handle
 *
 * @return true if "hgroup" is referenced
 */
static bool cqueue_is_hgroup_referenced(const ocs::tom::ConstElement<ocs::tom::CQ> this_elem,
                                        const ocs::tom::ConstElement<ocs::tom::HGRP> hgroup) {
   DENTER(CQUEUE_LAYER);

   bool ret = false;

   if (this_elem && hgroup) {
      const char *name = ocs::tom::get<HGRP_name>(hgroup);
      
      if (name != nullptr) {
         /*
          * Is it the host group carrying this queue's host list ... (N-T-5,
          * see cqueue_is_href_referenced() for why this is a name comparison)
          */
         dstring group_buffer = DSTRING_INIT;
         const char *own_group = hgroup_queue_group_name(ocs::tom::get<CQ_name>(this_elem),
                                                         &group_buffer);
         const bool is_own_group = (own_group != nullptr && strcmp(own_group, name) == 0);

         sge_dstring_free(&group_buffer);

         if (is_own_group) {
            ret = true;
         } else {
            /*
             * ... or is it contained on one of the attribute lists
             */
            int index = 0;
            while (cqueue_attribute_array[index].cqueue_attr != NoName) {
               // the attribute and its host field are given at run time, so the untyped access stays
               const lList *attr_list = lGetList(this_elem, cqueue_attribute_array[index].cqueue_attr);
               const lListElem *attr_elem = lGetElemHost(attr_list, cqueue_attribute_array[index].href_attr, name);

               if (attr_elem != nullptr) {
                  ret = true;
                  break;
               }
               index++;
            }
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Is one href referenced
 *
 * Returns true if at least one host contained in "href_list" is
 * referenced in the cluster queue "this_elem". If "only_hostlist"
 * is true then only the hostlist will be tested and all
 * parameter lists will be ignored.
 *
 * @param this_elem CQ_Type object, may be an empty handle
 * @param href_list HR_Type list, may be an empty handle
 * @param only_hostlist check only hostlist and ignore all parameter lists
 *
 * @return at least one object is referenced
 */
bool cqueue_is_a_href_referenced(const ocs::tom::ConstElement<ocs::tom::CQ> this_elem,
                                 const ocs::tom::ConstList<ocs::tom::HR> href_list, const bool only_hostlist) {
   DENTER(CQUEUE_LAYER);

   bool ret = false;

   if (this_elem && href_list) {
      for (const auto href : href_list) {
         if (cqueue_is_href_referenced(this_elem, href, only_hostlist)) {
            ret = true;
            break;
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief cqueue_is_a_href_referenced() for an untyped caller
 *
 * @param this_elem CQ_Type object, checked; may be nullptr
 * @param href_list HR_Type list, checked; may be nullptr
 * @param only_hostlist check only hostlist and ignore all parameter lists
 *
 * @return as cqueue_is_a_href_referenced()
 */
bool cqueue_is_a_href_referenced(const lListElem *this_elem, const lList *href_list, const bool only_hostlist) {
   return cqueue_is_a_href_referenced(ocs::tom::ConstElement<ocs::tom::CQ>(this_elem),
                                      ocs::tom::ConstList<ocs::tom::HR>(href_list), only_hostlist);
}

/**
 * @brief Set default attributes
 *
 * This function initializes all attributes of an empty cluster
 * queue with default values. Please note that "this_elem" has to
 * be "empty" before this function is called.
 *
 * The attribute lists are set through their field numbers, which
 * this function walks in tables; that part stays untyped.
 *
 * @param this_elem CQ_Type, may be an empty handle
 * @param answer_list AN_Type
 *
 * @return error state true  - success false - error
 */
bool cqueue_set_template_attributes(const ocs::tom::Element<ocs::tom::CQ> this_elem, lList **answer_list) {
   DENTER(CQUEUE_LAYER);

   bool ret = true;

   if (this_elem) {
      /*
       * initialize uint32_t values
       */
      if (ret) {
         const uint32_t value[] = {
            0, 1, 1, 0 
         }; 
         const int attr[] = {
            CQ_seq_no, CQ_nsuspend, CQ_job_slots, NoName
         };
         int index = 0;

         while (attr[index] != NoName) {
            lList *attr_list = nullptr;
            auto attr_elem = ocs::tom::emplace_back<AULNG_href>(&attr_list, HOSTREF_DEFAULT);

            ocs::tom::set<AULNG_value>(attr_elem, value[index]);
            lSetList(this_elem, attr[index], attr_list);
            index++;
         }
      }

      /*
       * qtype
       */
      if (ret) {
         const char *string = "BATCH INTERACTIVE";
         uint32_t value = 0;
         lList *attr_list = nullptr;
         auto attr_elem = ocs::tom::emplace_back<AQTLIST_href>(&attr_list, HOSTREF_DEFAULT);

         sge_parse_bitfield_str(string, queue_types, &value, "",
                                answer_list, true);

         ocs::tom::set<AQTLIST_value>(attr_elem, value);
         ocs::tom::set<CQ_qtype>(this_elem, attr_list);
      }

      /*
       * initialize bool values
       */
      if (ret) {
         lList *attr_list = nullptr;
         auto attr = ocs::tom::emplace_back<ABOOL_href>(&attr_list, HOSTREF_DEFAULT);

         ocs::tom::set<ABOOL_value>(attr, false);
         ocs::tom::set<CQ_rerun>(this_elem, attr_list);
      }

      /*
       * initialize memory values
       */
      if (ret) {
         const char *value[] = {
            INFINITY_STR, INFINITY_STR, INFINITY_STR, INFINITY_STR,
            INFINITY_STR, INFINITY_STR, INFINITY_STR, INFINITY_STR,
            INFINITY_STR, INFINITY_STR, INFINITY_STR, INFINITY_STR,
            nullptr
         };
         const int attr[] = {
            CQ_s_fsize, CQ_h_fsize, CQ_s_data, CQ_h_data,
            CQ_s_stack, CQ_h_stack, CQ_s_core, CQ_h_core,
            CQ_s_rss, CQ_h_rss, CQ_s_vmem, CQ_h_vmem,
            NoName
         };
         int index = 0;

         while (attr[index] != NoName) {
            lList *attr_list = nullptr;
            auto attr_elem = ocs::tom::emplace_back<AMEM_href>(&attr_list, HOSTREF_DEFAULT);

            ocs::tom::set<AMEM_value>(attr_elem, value[index]);
            lSetList(this_elem, attr[index], attr_list);
            index++;
         }
      }
      
      /*
       * initialize time values
       */
      if (ret) {
         const char *value[] = {
            INFINITY_STR, INFINITY_STR, INFINITY_STR, INFINITY_STR,
            nullptr
         };
         const int attr[] = {
            CQ_s_rt, CQ_h_rt, CQ_s_cpu, CQ_h_cpu,
            NoName
         };
         int index = 0;

         while (attr[index] != NoName) {
            lList *attr_list = nullptr;
            auto attr_elem = ocs::tom::emplace_back<ATIME_href>(&attr_list, HOSTREF_DEFAULT);

            ocs::tom::set<ATIME_value>(attr_elem, value[index]);
            lSetList(this_elem, attr[index], attr_list);
            index++;
         }
      }

      /*
       * initialize interval values
       */
      if (ret) {
         const char *value[] = {
            "00:05:00", "00:05:00", "00:00:60",
            nullptr
         }; 
         const int attr[] = {
            CQ_suspend_interval, CQ_min_cpu_interval, CQ_notify,
            NoName
         };
         int index = 0;

         while (attr[index] != NoName) {
            lList *attr_list = nullptr;
            auto attr_elem = ocs::tom::emplace_back<AINTER_href>(&attr_list, HOSTREF_DEFAULT);

            ocs::tom::set<AINTER_value>(attr_elem, value[index]);
            lSetList(this_elem, attr[index], attr_list);
            index++;
         }
      }

      /*
       * initialize string values
       */
      if (ret) {
         const char *value[] = {
            "/tmp", "/bin/sh", "NONE",
            "0", "UNDEFINED", "NONE",
            "NONE", "unix_behavior", "NONE",
            "NONE", "NONE", "NONE",
            "default", 
            nullptr
         }; 
         const int attr[] = {
            CQ_tmpdir, CQ_shell, CQ_calendar,
            CQ_priority, CQ_processors, CQ_prolog,
            CQ_epilog, CQ_shell_start_mode, CQ_starter_method,
            CQ_suspend_method, CQ_resume_method, CQ_terminate_method,
            CQ_initial_state,
            NoName
         };
         int index = 0;

         while (attr[index] != NoName) {
            lList *attr_list = nullptr;
            auto attr_elem = ocs::tom::emplace_back<ASTR_href>(&attr_list, HOSTREF_DEFAULT);

            ocs::tom::set<ASTR_value>(attr_elem, value[index]);
            lSetList(this_elem, attr[index], attr_list);
            index++;
         }
      }

      /*
       * initialize string-list values
       */
      if (ret) {
         const int attr[] = {
            CQ_pe_list, CQ_ckpt_list,
            NoName
         };
         int index = 0;
         lList *value[] = {
            nullptr, nullptr, nullptr
         };

         value[0] = lCreateList("", ST_Type);
         ocs::tom::emplace_back<ST_name>(&(value[0]), "make");

         while (attr[index] != NoName) {
            lList *attr_list = nullptr;
            auto attr_elem = ocs::tom::emplace_back<ASTRLIST_href>(&attr_list, HOSTREF_DEFAULT);

            ocs::tom::set<ASTRLIST_value>(attr_elem, value[index]);
            lSetList(this_elem, attr[index], attr_list);
            index++;
         }
      }

      /*
       * initialize AUSRLIST_Type-list values
       */
      if (ret) {
         const int attr[] = {
            CQ_owner_list, CQ_acl, CQ_xacl,
            NoName
         };
         int index = 0;

         while (attr[index] != NoName) {
            lList *attr_list = nullptr;
            auto attr_elem = ocs::tom::emplace_back<AUSRLIST_href>(&attr_list, HOSTREF_DEFAULT);

            ocs::tom::set<AUSRLIST_value>(attr_elem, nullptr);
            lSetList(this_elem, attr[index], attr_list);
            index++;
         }
      }

      /*
       * initialize APRJLIST_Type-list values (only sgeee mode)
       */
      if (ret) {
         const int attr[] = {
            CQ_projects, CQ_xprojects,
            NoName
         };
         int index = 0;

         while (attr[index] != NoName) {
            lList *attr_list = nullptr;
            auto attr_elem = ocs::tom::emplace_back<APRJLIST_href>(&attr_list, HOSTREF_DEFAULT);

            ocs::tom::set<APRJLIST_value>(attr_elem, nullptr);
            lSetList(this_elem, attr[index], attr_list);
            index++;
         }
      }

      /*
       * initialize ACELIST_Type-list values
       */
      if (ret) {
         const int attr[] = {
            CQ_load_thresholds, CQ_suspend_thresholds, 
            CQ_consumable_config_list,
            NoName
         };
         lList *value[] = {
            nullptr, nullptr, nullptr, nullptr
         };
         int index = 0;

         value[0] = lCreateList("", CE_Type);
         auto elem = ocs::tom::emplace_back<CE_name>(&(value[0]), "np_load_avg");
         ocs::tom::set<CE_stringval>(elem, "1.75");

         while (attr[index] != NoName) {
            lList *attr_list = nullptr;
            auto attr_elem = ocs::tom::emplace_back<ACELIST_href>(&attr_list, HOSTREF_DEFAULT);

            ocs::tom::set<ACELIST_value>(attr_elem, value[index]);
            lSetList(this_elem, attr[index], attr_list);
            index++;
         }
      }

      /*
       * initialize ASOLIST_Type-list values
       */
      if (ret) {
         const int attr[] = {
            CQ_subordinate_list,
            NoName
         };
         int index = 0;

         while (attr[index] != NoName) {
            lList *attr_list = nullptr;
            auto attr_elem = ocs::tom::emplace_back<ASOLIST_href>(&attr_list, HOSTREF_DEFAULT);

            ocs::tom::set<ASOLIST_value>(attr_elem, nullptr);
            lSetList(this_elem, attr[index], attr_list);
            index++;
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief cqueue_set_template_attributes() for an untyped caller
 *
 * @param this_elem CQ_Type, checked; may be nullptr
 * @param answer_list AN_Type
 *
 * @return as cqueue_set_template_attributes()
 */
bool cqueue_set_template_attributes(lListElem *this_elem, lList **answer_list) {
   return cqueue_set_template_attributes(ocs::tom::Element<ocs::tom::CQ>(this_elem), answer_list);
}

/**
 * @brief Find a cluster queue in list
 *
 * Find the cluster queue with name "name" in the list "this_list".
 *
 * @param this_list CQ_Type list, may be an empty handle
 * @param name cluster queue name
 *
 * @return cluster queue object, or an empty handle
 */
ocs::tom::Element<ocs::tom::CQ> cqueue_list_locate(const ocs::tom::ConstList<ocs::tom::CQ> this_list,
                                                   const char *name) {
   return ocs::tom::find_first_rw<CQ_name>(this_list, name);
}

/**
 * @brief cqueue_list_locate() for an untyped caller
 *
 * @param this_list CQ_Type list, checked; may be nullptr
 * @param name cluster queue name
 *
 * @return cluster queue object or nullptr
 */
lListElem *cqueue_list_locate(const lList *this_list, const char *name) {
   return cqueue_list_locate(ocs::tom::ConstList<ocs::tom::CQ>(this_list), name).raw();
}

/**
 * @brief Returns one qinstance from a cqueue
 *
 * Finds the queue instance locateted on the host "hostname" of a
 * given cluster queue "this_elem".
 *
 * @param this_elem CQ_Type object
 * @param hostname resolved hostname
 *
 * @return qinstance object, or an empty handle
 */
ocs::tom::Element<ocs::tom::QU> cqueue_locate_qinstance(const ocs::tom::ConstElement<ocs::tom::CQ> this_elem,
                                                        const char *hostname) {
   const auto qinstance_list = ocs::tom::get<CQ_qinstances>(this_elem);

   return qinstance_list_locate(qinstance_list, hostname, nullptr);
}

/**
 * @brief cqueue_locate_qinstance() for an untyped caller
 *
 * @param this_elem CQ_Type object, checked
 * @param hostname resolved hostname
 *
 * @return qinstance object or nullptr
 */
lListElem *cqueue_locate_qinstance(const lListElem *this_elem, const char *hostname) {
   return cqueue_locate_qinstance(ocs::tom::ConstElement<ocs::tom::CQ>(this_elem), hostname).raw();
}

/**
 * @brief Check all cluster queue attributes
 *
 * Check all cluster queue settings (and correct them if possible).
 *    - test that there is exact one default setting
 *    - check that there is only one setting for used hgroups/hosts
 *    - resolve hostnames
 *    - test attribute values
 *
 * The attributes are walked through #cqueue_attribute_array, by field
 * numbers given at run time, and the native verify functions keep their
 * untyped signature; those parts stay untyped.
 *
 * @param cqueue CQ_Type object to be verified, may be an empty handle
 * @param answer_list AN_Type list
 * @param reduced_elem reduced CQ_Type. Contains only those attributes to be checked; may be an empty handle
 * @param in_master true if this function is called in the master code
 * @param master_calendar_list the calendars an attribute may refer to
 * @param master_ckpt_list the checkpointing environments an attribute may refer to
 * @param master_pe_list the parallel environments an attribute may refer to
 * @param master_userset_list the usersets an attribute may refer to
 * @param master_project_list the projects an attribute may refer to
 * @param master_centry_list the complex entries an attribute may refer to
 * @param master_cqueue_list the cluster queues an attribute may refer to
 * @param master_hgroup_list the host groups an attribute may refer to
 *
 * @return error state true  - success false - error
 */
bool cqueue_verify_attributes(const ocs::tom::Element<ocs::tom::CQ> cqueue, lList **answer_list,
                              const ocs::tom::ConstElement<ocs::tom::CQ> reduced_elem, const bool in_master,
                              const ocs::tom::ConstList<ocs::tom::CAL> master_calendar_list,
                              const ocs::tom::ConstList<ocs::tom::CK> master_ckpt_list,
                              const ocs::tom::ConstList<ocs::tom::PE> master_pe_list,
                              const ocs::tom::ConstList<ocs::tom::US> master_userset_list,
                              const ocs::tom::ConstList<ocs::tom::PR> master_project_list,
                              const ocs::tom::ConstList<ocs::tom::CE> master_centry_list,
                              const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list,
                              const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list) {
   DENTER(CQUEUE_LAYER);

   bool ret = true;

   if (cqueue && reduced_elem) {
      int index = 0;

      while (cqueue_attribute_array[index].cqueue_attr != NoName && ret) {
         int pos = lGetPosViaElem(reduced_elem,
                                  cqueue_attribute_array[index].cqueue_attr, SGE_NO_ABORT);

         if (pos >= 0) {
            const lList *list = lGetList(cqueue, cqueue_attribute_array[index].cqueue_attr);

            /*
             * Configurations without default setting are rejected
             */
            if (ret) {
               const lListElem *elem = lGetElemHost(list, cqueue_attribute_array[index].href_attr, HOSTREF_DEFAULT);

               if (elem == nullptr) {
                  snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_CQUEUE_NODEFVALUE_S, cqueue_attribute_array[index].name);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
                  ret = false;
               } 
            }

            /*
             * Reject multiple settings for one domain/host
             * Resolve all hostnames
             * Verify host group names
             */
            if (ret) {
               for_each_rw_lv(elem, list) {
                  const char *hostname = nullptr;
                  const void *iterator = nullptr;
                  const lListElem *first_elem = nullptr;

                  hostname = lGetHost(elem, cqueue_attribute_array[index].href_attr);
                  first_elem = lGetElemHostFirst(list, cqueue_attribute_array[index].href_attr, hostname, &iterator);

                  if (elem != first_elem) {
                     snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_CQUEUE_MULVALNOTALLOWED_S, hostname);
                     answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
                     ret = false;
                     break;
                  }
                  if (ocs::is_hgroup_name(hostname)) {
                     /*
                      * CS-2677, N-O-4: the host group of a cluster queue is
                      * that queue's host set and belongs to nobody else.
                      * Checked on both sides of GDI, and independently of
                      * whether the group exists -- the name is reserved, so a
                      * missing one is still not something to write.
                      */
                     if (hgroup_is_queue_group(hostname)) {
                        ERROR(MSG_HGRP_QUEUEGROUP_NOREF_S, hostname);
                        answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                        ret = false;
                        break;
                     }
                     if (in_master && strcmp(hostname, HOSTREF_DEFAULT)) {
                        const auto hgroup = hgroup_list_locate(master_hgroup_list, hostname);

                        if (!hgroup) {
                           ERROR(MSG_CQUEUE_INVALIDDOMSETTING_SS, cqueue_attribute_array[index].name, hostname);
                           answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                           ret = false;
                           break;
                        } 
                     }
                  } else {
                     char resolved_name[CL_MAXHOSTNAMELEN+1];
                     int back = getuniquehostname(hostname, resolved_name, 0);

                     if (back == CL_RETVAL_OK) {
                        lSetHost(elem, cqueue_attribute_array[index].href_attr, resolved_name);
                     } else {
                        /*
                         * Due to CR 6319231, IZ 1760 this is allowed
                         */
                     }
                  }
               }
            }
      
            /*
             * Call native verify function if it is possible
             */
            if (ret && 
                cqueue_attribute_array[index].verify_function != nullptr &&
                (cqueue_attribute_array[index].verify_client || in_master)) {
               const lList *master_list;

               switch(cqueue_attribute_array[index].cqueue_attr) {
                  case CQ_calendar:
                     master_list = master_calendar_list;
                     break;
                  case CQ_ckpt_list:
                     master_list = master_ckpt_list;
                     break;
                  case CQ_pe_list:
                     master_list = master_pe_list;
                     break;
                  case CQ_acl:
                  case CQ_xacl:
                     master_list = master_userset_list;
                     break;
                  case CQ_projects:
                  case CQ_xprojects:
                     master_list = master_project_list;
                     break;
                  case CQ_consumable_config_list:
                     master_list = master_centry_list;
                     break;
                  case CQ_subordinate_list:
                     master_list = master_cqueue_list;
                     break;
                  default:
                     master_list = nullptr;
               }

               for_each_rw_lv(elem, list) {
                  ret &= cqueue_attribute_array[index].verify_function(cqueue, answer_list, elem, master_list);
               }
            }
         }
         
         index++;
      }
   }
   DRETURN(ret);
}

/**
 * @brief cqueue_verify_attributes() for an untyped caller
 *
 * @param cqueue CQ_Type object to be verified, checked; may be nullptr
 * @param answer_list AN_Type list
 * @param reduced_elem reduced CQ_Type, unchecked: request data; may be nullptr
 * @param in_master true if this function is called in the master code
 * @param master_calendar_list CAL_Type, checked; may be nullptr
 * @param master_ckpt_list CK_Type, checked; may be nullptr
 * @param master_pe_list PE_Type, checked; may be nullptr
 * @param master_userset_list US_Type, checked; may be nullptr
 * @param master_project_list PR_Type, checked; may be nullptr
 * @param master_centry_list CE_Type, checked; may be nullptr
 * @param master_cqueue_list CQ_Type, checked; may be nullptr
 * @param master_hgroup_list HGRP_Type, checked; may be nullptr
 *
 * @return as cqueue_verify_attributes()
 */
bool cqueue_verify_attributes(lListElem *cqueue, lList **answer_list, lListElem *reduced_elem, const bool in_master,
                              const lList *master_calendar_list, const lList *master_ckpt_list,
                              const lList *master_pe_list, const lList *master_userset_list,
                              const lList *master_project_list, const lList *master_centry_list,
                              const lList *master_cqueue_list, const lList *master_hgroup_list) {
   return cqueue_verify_attributes(ocs::tom::Element<ocs::tom::CQ>(cqueue), answer_list,
                                   ocs::tom::ConstElement<ocs::tom::CQ>::from_typed(reduced_elem), in_master,
                                   ocs::tom::ConstList<ocs::tom::CAL>(master_calendar_list),
                                   ocs::tom::ConstList<ocs::tom::CK>(master_ckpt_list),
                                   ocs::tom::ConstList<ocs::tom::PE>(master_pe_list),
                                   ocs::tom::ConstList<ocs::tom::US>(master_userset_list),
                                   ocs::tom::ConstList<ocs::tom::PR>(master_project_list),
                                   ocs::tom::ConstList<ocs::tom::CE>(master_centry_list),
                                   ocs::tom::ConstList<ocs::tom::CQ>(master_cqueue_list),
                                   ocs::tom::ConstList<ocs::tom::HGRP>(master_hgroup_list));
}

/**
 * @brief Find the cluster queues whose name matches a pattern
 *
 * Find all cqueues in "this_list" where the cqueue name matches
 * the pattern "cqueue_pattern" and return their names as queue
 * references.
 *
 * @param this_list CQ_Type, may be an empty handle
 * @param answer_list AN_Type; not written, the pattern is evaluated without an answer list
 * @param cqueue_pattern fnmatch pattern
 *
 * @return QR_Type list of the matching names; an empty handle if nothing matched or an argument is missing
 *
 * @note MT-NOTE: cqueue_list_find_all_matching_references() is MT safe
 */
ocs::tom::ListPtr<ocs::tom::QR>
cqueue_list_find_all_matching_references(const ocs::tom::ConstList<ocs::tom::CQ> this_list, lList **answer_list,
                                         const char *cqueue_pattern) {
   DENTER(CQUEUE_LAYER);

   // created by the first match, as the old out-parameter was
   ocs::tom::ListPtr<ocs::tom::QR> qref_list;

   if (this_list && cqueue_pattern != nullptr) {
      const bool cqueue_pattern_is_expression = ocs::is_expression(cqueue_pattern);

      for (const auto cqueue : this_list) {
         const char *cqueue_name = ocs::tom::get<CQ_name>(cqueue);
         /* use cqueue expression */
         if (!sge_eval_expression(ocs::CEntry::Type::STR, cqueue_pattern, cqueue_name, nullptr, true,
                                  cqueue_pattern_is_expression)) {
            if (!qref_list) {
               qref_list = ocs::tom::ListPtr<ocs::tom::QR>::create("");
            }
            qref_list->emplace_back<QR_name>(cqueue_name);
         }
      }
   }
   DRETURN(qref_list);
}

/**
 * @brief cqueue_list_find_all_matching_references() for an untyped caller
 *
 * Appends the matching names to "qref_list", creating it with the first
 * match, as the function did before it returned its result.
 *
 * @param this_list CQ_Type, checked; may be nullptr
 * @param answer_list AN_Type
 * @param cqueue_pattern fnmatch pattern
 * @param qref_list in-out: QR_Type list; nullptr makes the call a no-op
 *
 * @return always true
 */
bool cqueue_list_find_all_matching_references(const lList *this_list, lList **answer_list,
                                              const char *cqueue_pattern, lList **qref_list) {
   if (qref_list != nullptr) {
      auto found = cqueue_list_find_all_matching_references(ocs::tom::ConstList<ocs::tom::CQ>(this_list),
                                                            answer_list, cqueue_pattern);
      if (found) {
         if (*qref_list == nullptr) {
            *qref_list = found.release();
         } else {
            lList *found_raw = found.release();
            lAddList(*qref_list, &found_raw);
         }
      }
   }
   return true;
}

/**
 * @brief Move the attribute values of one queue request into another of the same cluster queue
 *
 * The values of each attribute are keyed by their host or domain reference; a value for a reference the target
 * already holds is a repeat and is dropped.
 *
 * @param target the request that is kept, CQ_Type
 * @param source the request whose values are moved; its attribute lists are left empty
 */
static void
ocs_cqueue_xattr_merge(lListElem *target, lListElem *source) {
   for (int index = 0; cqueue_attribute_array[index].cqueue_attr != NoName; index++) {
      const int attr = cqueue_attribute_array[index].cqueue_attr;
      const int href = cqueue_attribute_array[index].href_attr;
      lList *src = lGetListRW(source, attr);
      if (src == nullptr || lGetNumberOfElem(src) == 0) {
         continue;
      }
      lList *dst = lGetListRW(target, attr);
      if (dst == nullptr) {
         lList *moved = nullptr;
         lXchgList(source, attr, &moved);
         lSetList(target, attr, moved);
         continue;
      }
      while (lListElem *value = lFirstRW(src)) {
         lDechainElem(src, value);
         if (lGetElemHost(dst, href, lGetHost(value, href)) != nullptr) {
            lFreeElem(&value);
         } else {
            lAppendElem(dst, value);
         }
      }
   }
}

/**
 * @brief Prepare the queue requests of `qconf -[admr]attr queue` for the GDI request
 *
 * Each named target is one request. A target that names a queue instance or a queue domain
 * (`queue@host`, `queue@@hgroup`) gets the cluster queue name, and its attribute values, which must be default
 * entries, get the host or domain as their reference. Targets of the same cluster queue are merged into one
 * request, so that the cluster queue name, a unique key, is not repeated in the list; their values are told apart
 * by their reference.
 *
 * Each request is taken out of the list while it is renamed and put back, or merged, afterwards, so that a
 * rename never puts a name into the list a second time.
 *
 * @param this_list the requests, CQ_Type
 * @param answer_list AN_Type
 *
 * @return true on success, false on an error, described in @p answer_list
 *
 * @note MT-NOTE: cqueue_xattr_pre_gdi() is MT safe
 */
bool cqueue_xattr_pre_gdi(lList *this_list, lList **answer_list) {
   DENTER(CQUEUE_LAYER);

   bool ret = true;
   dstring cqueue_name = DSTRING_INIT;
   dstring host_domain = DSTRING_INIT;

   if (this_list != nullptr) {
      // the requests that are not yet prepared are at the front, the prepared ones are appended at the end
      for (int remaining = lGetNumberOfElem(this_list); remaining > 0 && ret; remaining--) {
         lListElem *cqueue = lDechainElem(this_list, lFirstRW(this_list));
         const char *name = ocs::tom::get<CQ_name>(cqueue);
         bool has_hostname = false;
         bool has_domain = false;

         if (!cqueue_name_split(name, &cqueue_name, &host_domain,
                           &has_hostname, &has_domain)) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX,
               ANSWER_QUALITY_ERROR, MSG_CQUEUE_NOQMATCHING_S, name);
            lAppendElem(this_list, cqueue);
            ret = false;
            break;
         }

         if (has_domain || has_hostname) {
            /*
             * Change QI/QD name to CQ name
             */
            ocs::tom::set<CQ_name>(cqueue, sge_dstring_get_string(&cqueue_name));

            /*
             * Make sure that there is only a default entry
             * and change that default entry to be a QD/QI entry
             */
            for (int index = 0; cqueue_attribute_array[index].cqueue_attr != NoName && ret; index++) {
               int pos = lGetPosViaElem(cqueue,
                                  cqueue_attribute_array[index].cqueue_attr, SGE_NO_ABORT);

               if (pos >= 0) {
                  lList *list = lGetPosList(cqueue, pos);

                  for_each_rw_lv(elem, list) {
                     const char *attr_hostname = lGetHost(elem, 
                                       cqueue_attribute_array[index].href_attr);

                     if (strcmp(HOSTREF_DEFAULT, attr_hostname)) {
                        snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_CQUEUE_NONDEFNOTALLOWED);
                        answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
                        ret = false;
                     } else {
                        lSetHost(elem, cqueue_attribute_array[index].href_attr,
                                 sge_dstring_get_string(&host_domain));
                     }
                  }
               }
            }
         }

         if (lListElem *target = ocs::tom::find_first_rw<CQ_name>(this_list, ocs::tom::get<CQ_name>(cqueue)); target != nullptr) {
            ocs_cqueue_xattr_merge(target, cqueue);
            lFreeElem(&cqueue);
         } else {
            lAppendElem(this_list, cqueue);
         }
      }
   }

   sge_dstring_free(&host_domain);
   sge_dstring_free(&cqueue_name);
   DRETURN(ret);
}

/**
 * @brief Checks for cqueue references
 *
 * The function goes through all cq subordinate definition and looks
 * for the cq_name handed in. If it is found, the function will return
 * true.
 *
 * @param cqueue_name cq name to look for
 * @param cqueue cq to look in (CQ_Type), may be an empty handle
 *
 * @return true - a reference was found
 *
 * @note MT-NOTE: cqueue_is_used_in_subordinate() is MT safe
 */
bool cqueue_is_used_in_subordinate(const char *cqueue_name, const ocs::tom::ConstElement<ocs::tom::CQ> cqueue) {
   DENTER(CQUEUE_LAYER);

   bool ret = false;

   if (cqueue && cqueue_name != nullptr) {
      for (const auto sub_el : ocs::tom::get<CQ_subordinate_list>(cqueue)) {
         const auto so = ocs::tom::find_first_in<ASOLIST_value, SO_name>(sub_el, cqueue_name);

         if (so) { /* we found a reference */
            ret = true;
            break;
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief cqueue_is_used_in_subordinate() for an untyped caller
 *
 * @param cqueue_name cq name to look for
 * @param cqueue cq to look in (CQ_Type), checked; may be nullptr
 *
 * @return as cqueue_is_used_in_subordinate()
 */
bool cqueue_is_used_in_subordinate(const char *cqueue_name, const lListElem *cqueue) {
   return cqueue_is_used_in_subordinate(cqueue_name, ocs::tom::ConstElement<ocs::tom::CQ>(cqueue));
}

/**
 * @brief Find hgroup references
 *
 * This function add each cqueue name contained in "this_list"
 * to "string_list" where "hgroup" is referenced. Errors will
 * be reported via "answer_list".
 *
 * @param this_list CQ_Type, may be an empty handle
 * @param answer_list AN_Type
 * @param hgroup HGRP_Type, may be an empty handle
 * @param string_list in-out: ST_Type list the names are appended to, created by the first one
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: cqueue_list_find_hgroup_references() is MT safe
 */
bool cqueue_list_find_hgroup_references(const ocs::tom::ConstList<ocs::tom::CQ> this_list, lList **answer_list,
                                        const ocs::tom::ConstElement<ocs::tom::HGRP> hgroup, lList **string_list) {
   DENTER(CQUEUE_LAYER);

   bool ret = true;

   if (this_list && hgroup && string_list != nullptr) {
      for (const auto cqueue : this_list) {
         if (cqueue_is_hgroup_referenced(cqueue, hgroup)) {
            const char *name = ocs::tom::get<CQ_name>(cqueue);

            ocs::tom::emplace_back<ST_name>(string_list, name);
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Tags each cqueue and optionally qinstance
 *
 * Tags all cqueues contained in "this_list" with the value
 * "tag_value". Optionally all qinstances contained in the
 * CQ_qinstances-sublist will be tagged too if "tag_qinstances"
 * is true.
 *
 * @param this_list CQ_Type, may be an empty handle
 * @param tag_value value
 * @param tag_qinstances true if instances should be tagged
 *
 * @note MT-NOTE: cqueue_list_set_tag() is MT safe
 */
void cqueue_list_set_tag(const ocs::tom::List<ocs::tom::CQ> this_list, const uint32_t tag_value,
                         const bool tag_qinstances) {
   DENTER(TOP_LAYER);
   for (const auto cqueue : this_list) {
      ocs::tom::set<CQ_tag>(cqueue, tag_value);
      if (tag_qinstances) {
         const auto qinstance_list = ocs::tom::get<CQ_qinstances>(cqueue);

         qinstance_list_set_tag(qinstance_list, tag_value);
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Finds a certain qinstance
 *
 * Returns a certain qinstance with the name "full_name" from
 * the master cqueue list given by "cqueue_list".
 *
 * @param cqueue_list CQ_Type, may be an empty handle
 * @param full_name qinstance name of the form `CQNAME@HOSTNAME`
 *
 * @return QU_Type element, or an empty handle
 *
 * @note MT-NOTE: cqueue_list_locate_qinstance() is MT safe
 */
ocs::tom::Element<ocs::tom::QU> cqueue_list_locate_qinstance(const ocs::tom::ConstList<ocs::tom::CQ> cqueue_list,
                                                             const char *full_name) {
   return cqueue_list_locate_qinstance_msg(cqueue_list, full_name, true);
}

/**
 * @brief cqueue_list_locate_qinstance() for an untyped caller
 *
 * @param cqueue_list CQ_Type, checked; may be nullptr
 * @param full_name qinstance name of the form `CQNAME@HOSTNAME`
 *
 * @return QU_Type element or nullptr
 */
lListElem *cqueue_list_locate_qinstance(const lList *cqueue_list, const char *full_name) {
   return cqueue_list_locate_qinstance_msg(ocs::tom::ConstList<ocs::tom::CQ>(cqueue_list), full_name, true).raw();
}

/**
 * @brief Finds a certain qinstance
 *
 * Returns a certain qinstance with the name "full_name" from
 * the master cqueue list given by "cqueue_list".
 *
 * @param cqueue_list CQ_Type, may be an empty handle
 * @param full_name qinstance name of the form `CQNAME@HOSTNAME`
 * @param raise_error true - show error messages - false - suppress error messages
 *
 * @return QU_Type element, or an empty handle
 *
 * @note MT-NOTE: cqueue_list_locate_qinstance_msg() is MT safe
 */
ocs::tom::Element<ocs::tom::QU> cqueue_list_locate_qinstance_msg(const ocs::tom::ConstList<ocs::tom::CQ> cqueue_list,
                                                                 const char *full_name, const bool raise_error) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::QU> ret = nullptr;

   if (full_name != nullptr) {
      dstring cqueue_name_buffer = DSTRING_INIT;
      dstring host_domain_buffer = DSTRING_INIT;
      bool has_hostname = false;
      bool has_domain = false;

      cqueue_name_split(full_name, &cqueue_name_buffer, &host_domain_buffer, &has_hostname, &has_domain);
      const char *cqueue_name = sge_dstring_get_string(&cqueue_name_buffer);
      const char *hostname = sge_dstring_get_string(&host_domain_buffer);
      const auto cqueue = ocs::tom::find_first<CQ_name>(cqueue_list, cqueue_name);
      if (cqueue) {
         const auto qinstance_list = ocs::tom::get<CQ_qinstances>(cqueue);

         ret = ocs::tom::find_first_rw<QU_qhostname>(qinstance_list, hostname);
      } else {
         if (raise_error) {
            ERROR(MSG_CQUEUE_CQUEUEISNULL_SSSII, full_name, cqueue_name != nullptr ? cqueue_name : "<null>",
                  hostname != nullptr ? hostname : "<null>", (int)has_hostname, (int)has_domain);
         }
      }
      sge_dstring_free(&cqueue_name_buffer);
      sge_dstring_free(&host_domain_buffer);
   } else {
      if (raise_error) {
         ERROR(SFNMAX, MSG_CQUEUE_FULLNAMEISNULL);
      }
   }
   DRETURN(ret);
}

/**
 * @brief cqueue_list_locate_qinstance_msg() for an untyped caller
 *
 * @param cqueue_list CQ_Type, checked; may be nullptr
 * @param full_name qinstance name of the form `CQNAME@HOSTNAME`
 * @param raise_error true - show error messages - false - suppress error messages
 *
 * @return QU_Type element or nullptr
 */
lListElem *cqueue_list_locate_qinstance_msg(const lList *cqueue_list, const char *full_name, const bool raise_error) {
   return cqueue_list_locate_qinstance_msg(ocs::tom::ConstList<ocs::tom::CQ>(cqueue_list), full_name, raise_error)
           .raw();
}

/**
 * @brief Purge attributes from queue
 *
 * The attribute lists other than the host list are reached through
 * #cqueue_attribute_array, by field numbers given at run time; that part
 * stays untyped.
 *
 * @param this_elem cqueue list element (CQ_Type), may be an empty handle
 * @param answer_list answer list
 * @param attr_list US_Type list with attributes to search for hgroup_or_hostname wildcards allowed
 * @param hgroup_or_hostname host group or hostname which should be purged
 *
 * @return true when hgroup_or_hostname was found and purged, false when no matching attribute
 *         holds it; an attribute without a list counts as not found (CS-3117)
 *
 * @note MT-NOTE: cqueue_purge_host() is not MT safe
 */
bool cqueue_purge_host(const ocs::tom::Element<ocs::tom::CQ> this_elem, lList **answer_list,
                       const ocs::tom::ConstList<ocs::tom::US> attr_list, const char *hgroup_or_hostname) {
   DENTER(CQUEUE_LAYER);

   bool ret = false;
   int index;

   lList *sublist = nullptr;

   if (this_elem) {
      for (const auto ep : attr_list) {
         const char *attr_name = ocs::tom::get<US_name>(ep);
         DPRINTF(SFQ "\n", attr_name);
      
         /* purge hostlist */         
         if (!sge_eval_expression(ocs::CEntry::Type::HOST, attr_name, SGE_ATTR_HOSTLIST, nullptr)) {
            sublist = nullptr;
            ocs::tom::exchange<CQ_hostlist>(this_elem, &sublist);
            // the removal reports success for a missing list as well, which is no deletion
            if (sublist != nullptr && ocs::tom::erase<HR_name>(&sublist, hgroup_or_hostname) == 1) {
               DPRINTF(SFQ " deleted in " SFQ "\n", hgroup_or_hostname, SGE_ATTR_HOSTLIST);
               ret = true;
            }
            ocs::tom::exchange<CQ_hostlist>(this_elem, &sublist);
         }
         
         /* purge attributes */ 
         index = 0;
         while(cqueue_attribute_array[index].name != nullptr) {

            /* Does the given attr_wildcard match with the actual attr_name */
            if (!sge_eval_expression(ocs::CEntry::Type::STR, attr_name, cqueue_attribute_array[index].name, nullptr)) {
               // the attribute and its host field are given at run time, so the untyped access stays.
               // The list is taken out of the element and put back like the host list above:
               // lDelElemHost() frees a list that becomes empty and resets only the pointer it was
               // given, so a copy of the field pointer left the element pointing at freed memory (CS-3117).
               sublist = nullptr;
               lXchgList(this_elem, cqueue_attribute_array[index].cqueue_attr, &sublist);
               if (sublist != nullptr &&
                   lDelElemHost(&sublist, cqueue_attribute_array[index].href_attr, hgroup_or_hostname) == 1) {
                  DPRINTF(SFQ " deleted in " SFQ "\n", hgroup_or_hostname, cqueue_attribute_array[index].name);
                  ret = true;
               }
               lXchgList(this_elem, cqueue_attribute_array[index].cqueue_attr, &sublist);
            }
            index++;
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief cqueue_purge_host() for an untyped caller
 *
 * @param this_elem cqueue list element (CQ_Type), checked; may be nullptr
 * @param answer_list answer list
 * @param attr_list US_Type list, checked; may be nullptr
 * @param hgroup_or_hostname host group or hostname which should be purged
 *
 * @return as cqueue_purge_host()
 */
bool cqueue_purge_host(lListElem *this_elem, lList **answer_list, lList *attr_list, const char *hgroup_or_hostname) {
   return cqueue_purge_host(ocs::tom::Element<ocs::tom::CQ>(this_elem), answer_list,
                            ocs::tom::ConstList<ocs::tom::US>(attr_list), hgroup_or_hostname);
}

/**
 * @brief Report configuration settings that look wrong but are not errors
 *
 * Backs `qconf -sick`. Warns about attribute settings that can never take
 * effect and about host group settings that do not cover every host in the
 * queue's host list.
 *
 * The attribute lists are walked through #cqueue_attribute_array, by field
 * numbers given at run time; that part stays untyped.
 *
 * @param cqueue the cluster queue to inspect (CQ_Type)
 * @param[out] answer_list receives error messages
 * @param master_hgroup_list the host groups, needed to resolve the host list
 * @param[out] ds receives the human readable report
 * @return true when the queue could be inspected
 */
bool cqueue_sick(const ocs::tom::ConstElement<ocs::tom::CQ> cqueue, lList **answer_list,
                 const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list, dstring *ds) {
   DENTER(TOP_LAYER);

   bool ret = true;

   /*
    * Warn about:
    *    - unused setting for attributes
    *    - hgroup settings were not all hosts are contained in hostlist
    */
   {
      const char *cqueue_name = ocs::tom::get<CQ_name>(cqueue);
      lList *used_hosts_raw = nullptr;    // HR_Type, written by href_list_find_all_references()
      lList *used_groups_raw = nullptr;   // HR_Type, written by href_list_find_all_references()
      lList **answer_list = nullptr;
      int index;
      
      /*
       * resolve href list of cqueue
       */
      href_list_find_all_references(ocs::tom::get<CQ_hostlist>(cqueue), answer_list,
                                    master_hgroup_list, &used_hosts_raw,
                                    &used_groups_raw);
      const auto used_hosts = ocs::tom::ListPtr<ocs::tom::HR>::from_typed(used_hosts_raw);
      const auto used_groups = ocs::tom::ListPtr<ocs::tom::HR>::from_typed(used_groups_raw);

      index = 0;
      while (cqueue_attribute_array[index].cqueue_attr != NoName) {
         /*
          * Skip geee attributes in ge mode
          */
         // the attribute and its host field are given at run time, so the untyped access stays
         const lList *attr_list = lGetList(cqueue, cqueue_attribute_array[index].cqueue_attr);
         const lListElem *next_attr = lFirst(attr_list);
         const lListElem *attr = nullptr;

         /*
          * Test each attribute setting if it is really used in the
          * current configuration
          */
         while((attr = next_attr) != nullptr) {
            const char *name = lGetHost(attr, cqueue_attribute_array[index].href_attr);

            next_attr = lNext(attr);
            if (ocs::is_hgroup_name(name)) {
               if (strcmp(name, HOSTREF_DEFAULT)) {
                  lList *used_hgroup_hosts_raw = nullptr;    // HR_Type, written by hgroup_find_all_references()
                  lList *used_hgroup_groups_raw = nullptr;   // HR_Type, written by hgroup_find_all_references()
                  lList *add_hosts_raw = nullptr;            // HR_Type, written by href_list_compare()
                  lList *equity_hosts_raw = nullptr;         // HR_Type, written by href_list_compare()

                  const auto hgroup = hgroup_list_locate(master_hgroup_list, name);

                  /*
                   * hgroup specific setting:
                   *    make sure each host of hgroup is part of 
                   *    resolved list
                   */
                  hgroup_find_all_references(hgroup, answer_list, 
                                             master_hgroup_list, 
                                             &used_hgroup_hosts_raw,
                                             &used_hgroup_groups_raw);
                  const auto used_hgroup_hosts = ocs::tom::ListPtr<ocs::tom::HR>::from_typed(used_hgroup_hosts_raw);
                  const auto used_hgroup_groups = ocs::tom::ListPtr<ocs::tom::HR>::from_typed(used_hgroup_groups_raw);
                  href_list_compare(*used_hgroup_hosts, answer_list,
                                    *used_hosts, &add_hosts_raw, nullptr,
                                    &equity_hosts_raw, nullptr);
                  const auto add_hosts = ocs::tom::ListPtr<ocs::tom::HR>::from_typed(add_hosts_raw);
                  const auto equity_hosts = ocs::tom::ListPtr<ocs::tom::HR>::from_typed(equity_hosts_raw);

                  if (!add_hosts->empty()) {
                     DTRACE;
                     sge_dstring_sprintf_append(ds, 
                             MSG_CQUEUE_DEFOVERWRITTEN_SSSSS,
                             cqueue_attribute_array[index].name,
                             name, cqueue_name, name, cqueue_name);
                     sge_dstring_append(ds, "\n");
                  }
               } else {
                  DTRACE;
               }
            } else {
               /*
                * host specific setting:
                *    make sure the host is contained in resolved list 
                */ 
               if (!href_list_has_member(*used_hosts, name)) {
                  DTRACE;
                  sge_dstring_sprintf_append(ds, 
                             MSG_CQUEUE_UNUSEDATTRSETTING_SS,
                             cqueue_attribute_array[index].name,
                             name, cqueue_name);
                  sge_dstring_append(ds, "\n");
               } else {
                  DTRACE;
               }
            }
         }
            
         index++;
      }
   }
   
   DRETURN(ret);
}
