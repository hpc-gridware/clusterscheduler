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
#include <fnmatch.h>

#include "uti/sge_dstring.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge.h"

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
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_subordinate.h"
#include "sgeobj/sge_eval_expression.h"
#include "sgeobj/msg_sgeobjlib.h"

#include "msg_clients_common.h"
#include "msg_common.h"

#define CQUEUE_LAYER TOP_LAYER

/* *INDENT-OFF* */

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

lEnumeration *
enumeration_create_reduced_cq(bool fetch_all_qi, bool fetch_all_nqi)
{
   lEnumeration *ret;
   dstring format_string = DSTRING_INIT;
   lDescr *descr = CQ_Type;
   int name_array[100];
   int names = -1;
   int attr;

   DENTER(CQUEUE_LAYER);
   for_each_attr(attr, descr) {
      if (names == -1) {
         sge_dstring_clear(&format_string);
         sge_dstring_append(&format_string, "%T(");
      }
      if ((attr == CQ_name) ||
          (fetch_all_qi && attr == CQ_qinstances) ||
          (fetch_all_nqi && attr != CQ_qinstances)) {
         names++;
         name_array[names] = attr;
         sge_dstring_append(&format_string, "%I");
      }
   }
   sge_dstring_append(&format_string, ")");
   ret = _lWhat(sge_dstring_get_string(&format_string), CQ_Type, 
                name_array, ++names);
   sge_dstring_free(&format_string);
   
   DRETURN(ret);
}

/****** sgeobj/cqueue/cqueue_name_split() *************************************
*  NAME
*     cqueue_name_split() -- Get the CQ and host part of a QI name 
*
*  SYNOPSIS
*     bool 
*     cqueue_name_split(const char *name, 
*                       dstring *cqueue_name, 
*                       dstring *host_domain, 
*                       bool *has_hostname, 
*                       bool *has_domain) 
*
*  FUNCTION
*     Splits a qinstance name into its components.
*
*     Examples:
*  
*     QI-name         cqueue_name  host_domain  has_hostname  has_domain
*     ------------------------------------------------------------------
*     all.q           all.q        ""           false         false
*     all.q@hostname  all.q        hostname     true          false
*     all.q@@hgrp     all.q        @hgrp        false         true
*
*  INPUTS
*     const char *name     - CQ/QD or QI name 
*     dstring *cqueue_name - CQ part of the name 
*     dstring *host_domain - host or hostgroup or nothing 
*     bool *has_hostname   - is "host_domain" a hostname 
*     bool *has_domain     - if "host_domain" a hostgroup 
*
*  RESULT
*     bool - error state
*     always true  - success
*******************************************************************************/
bool
cqueue_name_split(const char *name, 
                  dstring *cqueue_name, dstring *host_domain, 
                  bool *has_hostname, bool *has_domain)
{
   bool ret = true;

   DENTER(CQUEUE_LAYER);

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

/****** sge_cqueue/cqueue_get_name_from_qinstance() ****************************
*  NAME
*     cqueue_get_name_from_qinstance() -- returns the cluster queue part of a queue
*
*  SYNOPSIS
*     char* cqueue_get_name_from_qinstance(const char *queue_instance) 
*
*  FUNCTION
*     Returns a character pointer to a newly malloced string containing the cluster
*     queue part of a queue instance name.
*
*     The memory needs to be free'd by the caller
*
*  INPUTS
*     const char *queue_instance - queue instance or cluster queue
*
*  RESULT
*     char* - cluster queue name
*
*  NOTES
*     MT-NOTE: cqueue_get_name_from_qinstance() is MT safe 
*
*******************************************************************************/
char* cqueue_get_name_from_qinstance(const char *queue_instance)
{
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

/****** sgeobj/cqueue/cqueue_create() *****************************************
*  NAME
*     cqueue_create() -- Create a new cluster queue object 
*
*  SYNOPSIS
*     lListElem *
*     cqueue_create(lList **answer_list, 
*                   const char *name) 
*
*  FUNCTION
*     Returns a new cluster queue object with the name "name". 
*
*  INPUTS
*     lList **answer_list - AN_Type list 
*     const char *name    - cluster queue name 
*
*  RESULT
*     lListElem * - CQ_Type object or nullptr
*******************************************************************************/
lListElem *
cqueue_create(lList **answer_list, const char *name)
{
   lListElem *ret = nullptr;

   DENTER(CQUEUE_LAYER);
   if (name != nullptr) {
      ret = lCreateElem(CQ_Type);

      if (ret != nullptr) {
         lSetString(ret, CQ_name, name);
      } else {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_MEM_MEMORYALLOCFAILED_S, __func__);
         answer_list_add(answer_list, SGE_EVENT, STATUS_EMALLOC, ANSWER_QUALITY_ERROR);
      }
   }
   DRETURN(ret);
}

/****** sgeobj/cqueue/cqueue_is_href_referenced() *****************************
*  NAME
*     cqueue_is_href_referenced() -- is a host/hostgroup referenced in cqueue 
*
*  SYNOPSIS
*     bool 
*     cqueue_is_href_referenced(const lListElem *this_elem, 
*                               const lListElem *href, bool only_hostlist) 
*
*  FUNCTION
*     Is the given "href" (host or hostgroup referenece) used in the
*     definition of the cluster queue "this_elem"? If "only_hostlist" 
*     is true then only the hostlist will be tested and all
*     parameter lists will be ignored. 
*
*
*  INPUTS
*     const lListElem *this_elem - CQ_Type 
*     const lListElem *href      - HR_Type 
*     bool    only_hostlist      - check only hostlist and ignore 
*                                  all parameter lists
*
*  RESULT
*     bool - true if it is referenced
*******************************************************************************/
bool 
cqueue_is_href_referenced(const lListElem *this_elem, 
                          const lListElem *href, bool only_hostlist)
{
   bool ret = false;

   if (this_elem != nullptr && href != nullptr) {
      const char *href_name = lGetHost(href, HR_name);
      
      if (href_name != nullptr) {
         const lList *href_list = lGetList(this_elem, CQ_hostlist);
         const lListElem *tmp_href = lGetElemHost(href_list, HR_name, href_name);

         /*
          * Is the host group part of the hostlist definition ...
          */
         if (tmp_href != nullptr) {
            ret = true;
         }

         /*
          * ... or is it contained on one of the attribute lists
          */
         if (!only_hostlist) {
            int index = 0;

            while (cqueue_attribute_array[index].cqueue_attr != NoName && !ret) {
               const lList *attr_list = lGetList(this_elem, cqueue_attribute_array[index].cqueue_attr);
               const lListElem *attr_elem = lGetElemHost(attr_list, cqueue_attribute_array[index].href_attr, href_name);
                                                                                   
               if (attr_elem != nullptr) {
                  ret = true;
               }
               index++;
            }
         }
      }
   }
   return ret;
} 

/****** sgeobj/cqueue/cqueue_is_hgroup_referenced() ***************************
*  NAME
*     cqueue_is_hgroup_referenced() -- is a hgroup referenced in cqueue 
*
*  SYNOPSIS
*     bool 
*     cqueue_is_hgroup_referenced(const lListElem *this_elem, 
*                                 const lListElem *hgroup) 
*
*  FUNCTION
*     Is the given "hgroup" object referenced in the cluster queue
*     "this_elem".  
*
*  INPUTS
*     const lListElem *this_elem - CQ_Type 
*     const lListElem *hgroup    - HGRP_Type 
*
*  RESULT
*     bool - true if "hgroup" is referenced
*******************************************************************************/
bool 
cqueue_is_hgroup_referenced(const lListElem *this_elem, const lListElem *hgroup)
{
   bool ret = false;

   if (this_elem != nullptr && hgroup != nullptr) {
      const char *name = lGetHost(hgroup, HGRP_name);
      
      if (name != nullptr) {
         const lList *href_list = lGetList(this_elem, CQ_hostlist);
         const lListElem *tmp_href = lGetElemHost(href_list, HR_name, name);

         /*
          * Is the host group part of the hostlist definition ...
          */
         if (tmp_href != nullptr) {
            ret = true;
         } else {
            /*
             * ... or is it contained on one of the attribute lists
             */
            int index = 0;
            while (cqueue_attribute_array[index].cqueue_attr != NoName) {
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
   return ret;
}

/****** sgeobj/cqueue/cqueue_is_a_href_referenced() ***************************
*  NAME
*     cqueue_is_a_href_referenced() -- Is one href referenced 
*
*  SYNOPSIS
*     bool 
*     cqueue_is_a_href_referenced(const lListElem *this_elem, 
*                                 const lList *href_list, 
*                                 bool only_hostlist) 
*
*  FUNCTION
*     Returns true if at least one host contained in "href_list" is
*     referenced in the cluster queue "this_elem". If "only_hostlist" 
*     is true then only the hostlist will be tested and all
*     parameter lists will be ignored. 
*
*  INPUTS
*     const lListElem *this_elem - CQ_Type object
*     const lList *href_list     - HR_Type list 
*     bool only_hostlist         - check only hostlist and ignore 
*                                  all parameter lists
*
*  RESULT
*     bool - at least one object is referenced
*******************************************************************************/
bool 
cqueue_is_a_href_referenced(const lListElem *this_elem, 
                            const lList *href_list, bool only_hostlist)
{
   bool ret = false;
  
   if (this_elem != nullptr && href_list != nullptr) {
      const lListElem *href;

      for_each_ep(href, href_list) {
         if (cqueue_is_href_referenced(this_elem, href, only_hostlist)) {
            ret = true;
            break;
         }
      }
   }
   return ret;
} 

/****** sgeobj/cqueue/cqueue_set_template_attributes() ************************
*  NAME
*     cqueue_set_template_attributes() -- Set default attributes 
*
*  SYNOPSIS
*     bool 
*     cqueue_set_template_attributes(lListElem *this_elem, 
*                                    lList **answer_list) 
*
*  FUNCTION
*     This function initializes all attributes of an empty cluster
*     queue with default values. Please note that "this_elem" has to
*     be "empty" before this function is called.  
*
*  INPUTS
*     lListElem *this_elem - CQ_Type 
*     lList **answer_list  - AN_Type 
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error
*******************************************************************************/
bool
cqueue_set_template_attributes(lListElem *this_elem, lList **answer_list)
{
   bool ret = true;

   DENTER(CQUEUE_LAYER);
   if (this_elem != nullptr) {
      /*
       * initialize u_long32 values
       */
      if (ret) {
         const u_long32 value[] = {
            0, 1, 1, 0 
         }; 
         const int attr[] = {
            CQ_seq_no, CQ_nsuspend, CQ_job_slots, NoName
         };
         int index = 0;

         while (attr[index] != NoName) {
            lList *attr_list = nullptr;
            lListElem *attr_elem = lAddElemHost(&attr_list, AULNG_href, 
                                                HOSTREF_DEFAULT, AULNG_Type);

            lSetUlong(attr_elem, AULNG_value, value[index]);
            lSetList(this_elem, attr[index], attr_list);
            index++;
         }
      }

#if 0
      /*
       * initialize u_long32 values (SGEEE attributes)
       */
      if (ret ) {
         const u_long32 value[] = {
            0, 0, 0 
         }; 
         const int attr[] = {
            CQ_fshare, CQ_oticket, NoName
         };
         int index = 0;

         while (attr[index] != NoName) {
            lList *attr_list = nullptr;
            lListElem *attr_elem = lAddElemHost(&attr_list, AULNG_href, 
                                                HOSTREF_DEFAULT, AULNG_Type);

            lSetUlong(attr_elem, AULNG_value, value[index]);
            lSetList(this_elem, attr[index], attr_list);
            index++;
         }
      }
#endif

      /*
       * qtype
       */
      if (ret) {
         const char *string = "BATCH INTERACTIVE";
         u_long32 value = 0;
         lList *attr_list = nullptr;
         lListElem *attr_elem = lAddElemHost(&attr_list, AQTLIST_href,
                                             HOSTREF_DEFAULT, AQTLIST_Type);

         sge_parse_bitfield_str(string, queue_types, &value, "",
                                answer_list, true);

         lSetUlong(attr_elem, AQTLIST_value, value);
         lSetList(this_elem, CQ_qtype, attr_list);
      }

      /*
       * initialize bool values
       */
      if (ret) {
         lList *attr_list = nullptr;
         lListElem *attr = lAddElemHost(&attr_list, ABOOL_href, 
                                        HOSTREF_DEFAULT, ABOOL_Type);

         lSetBool(attr, ABOOL_value, false);
         lSetList(this_elem, CQ_rerun, attr_list);
      }

      /*
       * initialize memory values
       */
      if (ret) {
         const char *value[] = {
            "INFINITY", "INFINITY", "INFINITY", "INFINITY",
            "INFINITY", "INFINITY", "INFINITY", "INFINITY",
            "INFINITY", "INFINITY", "INFINITY", "INFINITY",
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
            lListElem *attr_elem = lAddElemHost(&attr_list, AMEM_href, 
                                                HOSTREF_DEFAULT, AMEM_Type);

            lSetString(attr_elem, AMEM_value, value[index]);
            lSetList(this_elem, attr[index], attr_list);
            index++;
         }
      }
      
      /*
       * initialize time values
       */
      if (ret) {
         const char *value[] = {
            "INFINITY", "INFINITY", "INFINITY", "INFINITY",
            nullptr
         }; 
         const int attr[] = {
            CQ_s_rt, CQ_h_rt, CQ_s_cpu, CQ_h_cpu,
            NoName
         };
         int index = 0;

         while (attr[index] != NoName) {
            lList *attr_list = nullptr;
            lListElem *attr_elem = lAddElemHost(&attr_list, ATIME_href, 
                                                HOSTREF_DEFAULT, ATIME_Type);

            lSetString(attr_elem, ATIME_value, value[index]);
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
            lListElem *attr_elem = lAddElemHost(&attr_list, AINTER_href, 
                                                HOSTREF_DEFAULT, AINTER_Type);

            lSetString(attr_elem, AINTER_value, value[index]);
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
            lListElem *attr_elem = lAddElemHost(&attr_list, ASTR_href, 
                                                HOSTREF_DEFAULT, ASTR_Type);

            lSetString(attr_elem, ASTR_value, value[index]);
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
         lAddElemStr(&(value[0]), ST_name, "make", ST_Type);

         while (attr[index] != NoName) {
            lList *attr_list = nullptr;
            lListElem *attr_elem = lAddElemHost(&attr_list, ASTRLIST_href, 
                                                HOSTREF_DEFAULT, ASTRLIST_Type);

            lSetList(attr_elem, ASTRLIST_value, value[index]);
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
            lListElem *attr_elem = lAddElemHost(&attr_list, AUSRLIST_href, 
                                                HOSTREF_DEFAULT, AUSRLIST_Type);

            lSetList(attr_elem, AUSRLIST_value, nullptr);
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
            lListElem *attr_elem = lAddElemHost(&attr_list, APRJLIST_href, 
                                                HOSTREF_DEFAULT, APRJLIST_Type);

            lSetList(attr_elem, APRJLIST_value, nullptr);
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
         lListElem *elem;

         value[0] = lCreateList("", CE_Type);
         elem = lAddElemStr(&(value[0]), CE_name, "np_load_avg", CE_Type); 
         lSetString(elem, CE_stringval, "1.75");

         while (attr[index] != NoName) {
            lList *attr_list = nullptr;
            lListElem *attr_elem = lAddElemHost(&attr_list, ACELIST_href, 
                                                HOSTREF_DEFAULT, ACELIST_Type);

            lSetList(attr_elem, ACELIST_value, value[index]);
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
            lListElem *attr_elem = lAddElemHost(&attr_list, ASOLIST_href, 
                                                HOSTREF_DEFAULT, ASOLIST_Type);

            lSetList(attr_elem, ASOLIST_value, nullptr);
            lSetList(this_elem, attr[index], attr_list);
            index++;
         }
      }
   }
   DRETURN(ret);
}

/****** sgeobj/cqueue/cqueue_list_add_cqueue() ********************************
*  NAME
*     cqueue_list_add_cqueue() -- Add a cluster queue to its master list 
*
*  SYNOPSIS
*     bool 
*     cqueue_list_add_cqueue(lListElem *queue) 
*
*  FUNCTION
*     Add a cluster queue in its master list. 
*
*  INPUTS
*     lListElem *queue - CQ_Type 
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error
*******************************************************************************/
bool
cqueue_list_add_cqueue(lList *this_list, lListElem *queue)
{
   bool ret = false;
   static lSortOrder *so = nullptr;

   DENTER(TOP_LAYER);

   if (queue != nullptr) {
      if (so == nullptr) {
         so = lParseSortOrderVarArg(CQ_Type, "%I+", CQ_name);
      }

      lInsertSorted(so, queue, this_list);
      ret = true;
   } 
   DRETURN(ret);
}

/****** sgeobj/cqueue/cqueue_list_locate() ************************************
*  NAME
*     cqueue_list_locate() -- Find a cluster queue in list 
*
*  SYNOPSIS
*     lListElem * 
*     cqueue_list_locate(const lList *this_list, 
*                        const char *name) 
*
*  FUNCTION
*    Find the cluster queue with name "name" in the list "this_list". 
*
*  INPUTS
*     const lList *this_list - CQ_Type list 
*     const char *name       - cluster queue name 
*
*  RESULT
*     lListElem * - cluster queue object or nullptr
*******************************************************************************/
lListElem *
cqueue_list_locate(const lList *this_list, const char *name)
{
   return lGetElemStrRW(this_list, CQ_name, name);
}

/****** sgeobj/cqueue/cqueue_locate_qinstance() *******************************
*  NAME
*     cqueue_locate_qinstance() -- returns one qinstance from a cqueue 
*
*  SYNOPSIS
*     lListElem * 
*     cqueue_locate_qinstance(const lListElem *this_elem, 
*                             const char *hostname) 
*
*  FUNCTION
*     Finds the queue instance locateted on the host "hostname" of a
*     given cluster queue "this_elem". 
*
*  INPUTS
*     const lListElem *this_elem - CQ_Type object 
*     const char *hostname       - resolved hostname  
*
*  RESULT
*     lListElem * - qinstance object or nullptr
*******************************************************************************/
lListElem *
cqueue_locate_qinstance(const lListElem *this_elem, const char *hostname)
{
   const lList *qinstance_list = lGetList(this_elem, CQ_qinstances);

   return qinstance_list_locate(qinstance_list, hostname, nullptr);
}

/****** sgeobj/cqueue/cqueue_verify_attributes() ******************************
*  NAME
*     cqueue_verify_attributes() -- check all cluster queue attributes 
*
*  SYNOPSIS
*     bool 
*     cqueue_verify_attributes(lListElem *cqueue, 
*                              lList **answer_list, 
*                              lListElem *reduced_elem, 
*                              bool in_master) 
*
*  FUNCTION
*     Check all cluster queue settings (and correct them if possible).
*
*        - test that there is exact one default setting 
*        - check that there is only one setting for used hgroups/hosts
*        - resolve hostnames
*        - test attribute values  
*
*  INPUTS
*     lListElem *cqueue       - CQ_Type object to be verified 
*     lList **answer_list     - AN_Type list 
*     lListElem *reduced_elem - reduced CQ_Type. Containes
*                               only those attributes to be checked  
*     bool in_master          - true if this function is called in the
*                               master code 
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error
*******************************************************************************/
bool 
cqueue_verify_attributes(lListElem *cqueue, lList **answer_list,
                         lListElem *reduced_elem, bool in_master, const lList *master_calendar_list, 
                         const lList *master_ckpt_list, const lList *master_pe_list, const lList *master_userset_list, 
                         const lList *master_project_list, const lList *master_centry_list, const lList *master_cqueue_list,
                         const lList *master_hgroup_list)
{
   bool ret = true;

   DENTER(CQUEUE_LAYER);
   if (cqueue != nullptr && reduced_elem != nullptr) {
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
               lListElem *elem = nullptr;

               for_each_rw(elem, list) {
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
                  if (is_hgroup_name(hostname)) {
                     if (in_master && strcmp(hostname, HOSTREF_DEFAULT)) {
                        const lListElem *hgroup = hgroup_list_locate(master_hgroup_list, hostname);

                        if (hgroup == nullptr) {
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
               lListElem *elem = nullptr;
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

               for_each_rw(elem, list) {
                  ret &= cqueue_attribute_array[index].verify_function(cqueue, answer_list, elem, master_list);
               }
            }
         }
         
         index++;
      }
   }
   DRETURN(ret);
}

/****** sgeobj/cqueue/cqueue_list_find_all_matching_references() **************
*  NAME
*     cqueue_list_find_all_matching_references() -- as it says 
*
*  SYNOPSIS
*     bool 
*     cqueue_list_find_all_matching_references(const lList *this_list, 
*                                              lList **answer_list, 
*                                              const char *cqueue_pattern, 
*                                              lList **qref_list) 
*
*  FUNCTION
*     Find all cqueues in "this_list" where the cqueue name matches 
*     the pattern "cqueue_pattern". The names of that cqueues will
*     be added to the sublist "qref_list" (QR_Type). "answer_list" will
*     contain an error message if this function failes.
*
*  INPUTS
*     const lList *this_list     - CQ_Type 
*     lList **answer_list        - AN_Type 
*     const char *cqueue_pattern - fnmatch patterm 
*     lList **qref_list          - QR_Type list 
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error 
*
*  NOTES
*     MT-NOTE: cqueue_list_find_all_matching_references() is MT safe 
*******************************************************************************/
bool
cqueue_list_find_all_matching_references(const lList *this_list,
                                         lList **answer_list,
                                         const char *cqueue_pattern,
                                         lList **qref_list)
{
   bool ret = true;

   DENTER(CQUEUE_LAYER);
   if (this_list != nullptr && cqueue_pattern != nullptr && qref_list != nullptr) {
      const lListElem *cqueue;

      for_each_ep(cqueue, this_list) {
         const char *cqueue_name = lGetString(cqueue, CQ_name);
         /* use cqueue expression */         
         if (!sge_eval_expression(TYPE_STR, cqueue_pattern, cqueue_name, nullptr)) {
            if (*qref_list == nullptr) {
               *qref_list = lCreateList("", QR_Type);
            }
            if (*qref_list != nullptr) {
               lAddElemStr(qref_list, QR_name, cqueue_name, QR_Type);
            }
         }
      }
   }
   DRETURN(ret);
}

/****** sgeobj/cqueue/cqueue_xattr_pre_gdi() **********************************
*  NAME
*     cqueue_xattr_pre_gdi() --  
*
*  SYNOPSIS
*     bool 
*     cqueue_xattr_pre_gdi(lList *this_list, lList **answer_list) 
*
*  FUNCTION
*     This function makes sure that a cqueue elements has the necessary
*     information before it is sent to qmaster as a modify gdi request
*
*  INPUTS
*     lList *this_list    - CQ_Type 
*     lList **answer_list - AN_Type 
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error ("answer_list" containes more information
*
*  NOTES
*     MT-NOTE: cqueue_xattr_pre_gdi() is MT safe 
*******************************************************************************/
bool
cqueue_xattr_pre_gdi(lList *this_list, lList **answer_list) 
{
   bool ret = true;
   dstring cqueue_name = DSTRING_INIT;
   dstring host_domain = DSTRING_INIT;

   DENTER(CQUEUE_LAYER);
   if (this_list != nullptr) {
      lListElem *cqueue = nullptr;
   
      for_each_rw(cqueue, this_list) {
         const char *name = lGetString(cqueue, CQ_name);
         bool has_hostname = false;
         bool has_domain = false;

         if (!cqueue_name_split(name, &cqueue_name, &host_domain,
                           &has_hostname, &has_domain)) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX,
               ANSWER_QUALITY_ERROR, MSG_CQUEUE_NOQMATCHING_S, name);
            ret = false;
            break;
         }

         if (has_domain || has_hostname) {
            int index = 0;

            /*
             * Change QI/QD name to CQ name
             */
            lSetString(cqueue, CQ_name, sge_dstring_get_string(&cqueue_name));

            /*
             * Make sure that there is only a default entry
             * and change that default entry to be a QD/QI entry
             */
            while (cqueue_attribute_array[index].cqueue_attr != NoName && ret) {
               int pos = lGetPosViaElem(cqueue,
                                  cqueue_attribute_array[index].cqueue_attr, SGE_NO_ABORT);

               if (pos >= 0) {
                  lList *list = lGetPosList(cqueue, pos);
                  lListElem *elem = nullptr;

                  for_each_rw(elem, list) {
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
               index++;
            }
         }
      }
   }

   sge_dstring_free(&host_domain);
   sge_dstring_free(&cqueue_name);
   DRETURN(ret);
}

/****** sgeobj/cqueue_is_used_in_subordinate() ****************************
*  NAME
*     cqueue_is_used_in_subordinate() -- checks for cqueue references
*
*  SYNOPSIS
*     bool cqueue_is_used_in_subordinate(const char *cqueue_name, 
*                                        lListElem *cqueue)
*
*  FUNCTION
*     The function goes through all cq subordinate definition and looks
*     for the cq_name handed in. If it is found, the function will return
*     true.
*
*  INPUTS
*     const char *cqueue_name - cq name to look for
*     const lListElem *cqueue - cq to look in
*     
*
*  RESULT
*     bool - true - a reference was found     
*
*  NOTES
*     MT-NOTE: cqueue_is_used_in_subordinate() is MT safe 
*******************************************************************************/
bool
cqueue_is_used_in_subordinate(const char *cqueue_name, const lListElem *cqueue)
{
   bool ret = false;

   DENTER(CQUEUE_LAYER);

   if (cqueue != nullptr && cqueue_name != nullptr) {
      const lList *sub_list = lGetList(cqueue, CQ_subordinate_list);
      const lListElem *sub_el;
      const lListElem *so;

      for_each_ep(sub_el, sub_list) {
         so = lGetSubStr(sub_el, SO_name, cqueue_name, ASOLIST_value);

         if (so != nullptr) { /* we found a reference */
            ret = true;
            break;
         }
      }
   }

   DRETURN(ret);
}

/****** sgeobj/cqueue/cqueue_list_find_hgroup_references() ********************
*  NAME
*     cqueue_list_find_hgroup_references() -- find hgroup references 
*
*  SYNOPSIS
*     bool 
*     cqueue_list_find_hgroup_references(const lList *this_list, 
*                                        lList **answer_list, 
*                                        const lListElem *hgroup, 
*                                        lList **string_list) 
*
*  FUNCTION
*     This function add each cqueue name contained in "this_list" 
*     to "string_list" where "hgroup" is referenced. Errors will
*     be reported via "answer_list".
*
*  INPUTS
*     const lList *this_list  - CQ_Type 
*     lList **answer_list     - AN_Type 
*     const lListElem *hgroup - HGRP_Type 
*     lList **string_list     - ST_Type 
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error
*
*  NOTES
*     MT-NOTE: cqueue_list_find_hgroup_references() is MT safe 
*******************************************************************************/
bool
cqueue_list_find_hgroup_references(const lList *this_list, lList **answer_list,
                                   const lListElem *hgroup, lList **string_list)
{
   bool ret = true;
   const lListElem *cqueue;

   DENTER(CQUEUE_LAYER);
   if (this_list != nullptr && hgroup != nullptr && string_list != nullptr) {
      for_each_ep(cqueue, this_list) {
         if (cqueue_is_hgroup_referenced(cqueue, hgroup)) {
            const char *name = lGetString(cqueue, CQ_name);

            lAddElemStr(string_list, ST_name, name, ST_Type);
         }
      }
   }
   DRETURN(ret);
}

/****** sgeobj/cqueue/cqueue_list_set_tag() ***********************************
*  NAME
*     cqueue_list_set_tag() -- tags each cqueue and optionally qinstance 
*
*  SYNOPSIS
*     void 
*     cqueue_list_set_tag(lList *this_list, 
*                         u_long32 tag_value, bool tag_qinstances) 
*
*  FUNCTION
*     Tags all cqueues contained in "this_list" with the value 
*     "tag_value". Optionally all qinstances contained in the
*     CQ_qinstances-sublist will be tagged too if "tag_qinstances" 
*     is true. 
*
*  INPUTS
*     lList *this_list    - CQ_Type 
*     u_long32 tag_value  - value 
*     bool tag_qinstances - true if instances should be tagged 
*
*  RESULT
*     void - None
*
*  NOTES
*     MT-NOTE: cqueue_list_set_tag() is MT safe 
*******************************************************************************/
void
cqueue_list_set_tag(lList *this_list, u_long32 tag_value, bool tag_qinstances)
{
   DENTER(TOP_LAYER);
   if (this_list != nullptr) {
      lListElem *cqueue = nullptr;

      for_each_rw(cqueue, this_list) {
         lSetUlong(cqueue, CQ_tag, tag_value);
         if (tag_qinstances) {
            lList *qinstance_list = lGetListRW(cqueue, CQ_qinstances);

            qinstance_list_set_tag(qinstance_list, tag_value);
         }
      }
   }
   DRETURN_VOID;
}

/****** sgeobj/cqueue/cqueue_list_locate_qinstance() **************************
*  NAME
*     cqueue_list_locate_qinstance() -- finds a certain qinstance 
*
*  SYNOPSIS
*     lListElem * 
*     cqueue_list_locate_qinstance(lList *cqueue_list, const char *full_name) 
*
*  FUNCTION
*     Returns a certain qinstance with the name "full_name" from
*     the master cqueue list given by "cqueue_list". 
*
*  INPUTS
*     lList *cqueue_list    - CQ_Type 
*     const char *full_name - qinstance name of the form <CQNAME>@<HOSTNAME> 
*
*  RESULT
*     lListElem * - QU_Type
*
*  NOTES
*     MT-NOTE: cqueue_list_locate_qinstance() is MT safe 
*******************************************************************************/
lListElem *
cqueue_list_locate_qinstance(const lList *cqueue_list, const char *full_name)
{
   return cqueue_list_locate_qinstance_msg(cqueue_list, full_name, true);
}

/****** sgeobj/cqueue/cqueue_list_locate_qinstance_msg() ***********************
*  NAME
*     cqueue_list_locate_qinstance_msg() -- finds a certain qinstance 
*
*  SYNOPSIS
*     lListElem * 
*     cqueue_list_locate_qinstance_msg(lList *cqueue_list, const char *full_name, bool raise_error) 
*
*  FUNCTION
*     Returns a certain qinstance with the name "full_name" from
*     the master cqueue list given by "cqueue_list". 
*
*  INPUTS
*     lList *cqueue_list    - CQ_Type 
*     const char *full_name - qinstance name of the form <CQNAME>@<HOSTNAME> 
*     bool raise_error      - true - show error messages
*                           - false - suppress error messages
*
*  RESULT
*     lListElem * - QU_Type
*
*  NOTES
*     MT-NOTE: cqueue_list_locate_qinstance_msg() is MT safe 
*******************************************************************************/
lListElem *
cqueue_list_locate_qinstance_msg(const lList *cqueue_list, const char *full_name, bool raise_error) 
{
   lListElem *ret = nullptr;

   DENTER(TOP_LAYER);
   if (full_name != nullptr) {
      dstring cqueue_name_buffer = DSTRING_INIT;
      dstring host_domain_buffer = DSTRING_INIT;
      bool has_hostname = false;
      bool has_domain = false;

      cqueue_name_split(full_name, &cqueue_name_buffer, &host_domain_buffer, &has_hostname, &has_domain);
      const char *cqueue_name = sge_dstring_get_string(&cqueue_name_buffer);
      const char *hostname = sge_dstring_get_string(&host_domain_buffer);
      const lListElem *cqueue = lGetElemStr(cqueue_list, CQ_name, cqueue_name);
      if (cqueue != nullptr) {
         lList *qinstance_list = lGetListRW(cqueue, CQ_qinstances);

         ret = lGetElemHostRW(qinstance_list, QU_qhostname, hostname);
      } else {
         if (raise_error) {
            ERROR(MSG_CQUEUE_CQUEUEISNULL_SSSII, full_name, cqueue_name != nullptr ? cqueue_name : "<null>", hostname != nullptr ? hostname: "<null>", (int)has_hostname, (int)has_domain);
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

/****** sge_cqueue/cqueue_find_used_href() *************************************
*  NAME
*     cqueue_find_used_href() -- Finds used host references  
*
*  SYNOPSIS
*     bool 
*     cqueue_find_used_href(lListElem *this_elem, 
*                           lList **answer_list, 
*                           lList **href_list) 
*
*  FUNCTION
*     This function returns all host references in "href_list"
*     for which attribute specific overwritings exist in the cqueue 
*     "this_elem". 
*
*  INPUTS
*     lListElem *this_elem - CQ_Type 
*     lList **answer_list  - AN_Type 
*     lList **href_list    - HR_Type 
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error
*
*  NOTES
*     MT-NOTE: cqueue_find_used_href() is MT safe 
*******************************************************************************/
bool
cqueue_find_used_href(lListElem *this_elem, lList **answer_list, 
                      lList **href_list) 
{
   bool ret = true;

   DENTER(CQUEUE_LAYER);
   if (this_elem != nullptr) {
      int index=0;

      while (cqueue_attribute_array[index].cqueue_attr != NoName && ret) {
         int pos = lGetPosViaElem(this_elem,
                            cqueue_attribute_array[index].cqueue_attr, SGE_NO_ABORT);

         if (pos >= 0) {
            lList *list = lGetPosList(this_elem, pos);
            const lListElem *elem = nullptr;

            for_each_ep(elem, list) {
               const char *attr_hostname = lGetHost(elem, 
                                 cqueue_attribute_array[index].href_attr);

               ret = href_list_add(href_list, answer_list, attr_hostname);
            }
         }
         index++;
      }
   }
   DRETURN(ret);
}

/****** sgeobj/cqueue/cqueue_trash_used_href_setting() ************************
*  NAME
*     cqueue_trash_used_href_setting() -- trash certain setting 
*
*  SYNOPSIS
*     bool 
*     cqueue_trash_used_href_setting(lListElem *this_elem, 
*                                    lList **answer_list, 
*                                    const char *hgroup_or_hostname) 
*
*  FUNCTION
*     Trash all attribute specific overwritings in "this_elem" for
*     the give host or hgroup "hgroup_or_hostname". Errors will be 
*     reported in "answer_list". 
*
*  INPUTS
*     lListElem *this_elem           - CQ_Type 
*     lList **answer_list            - AN_Type 
*     const char *hgroup_or_hostname - host or hgroup name 
*
*  RESULT
*     bool - error result
*        true  - success
*        false - error
*
*  NOTES
*     MT-NOTE: cqueue_trash_used_href_setting() is MT safe 
*******************************************************************************/
bool
cqueue_trash_used_href_setting(lListElem *this_elem, lList **answer_list, 
                               const char *hgroup_or_hostname) 
{
   bool ret = true;

   DENTER(CQUEUE_LAYER);
   if (this_elem != nullptr) {
      int index=0;

      while (cqueue_attribute_array[index].cqueue_attr != NoName && ret) {
         int pos = lGetPosViaElem(this_elem,
                            cqueue_attribute_array[index].cqueue_attr, SGE_NO_ABORT);

         if (pos >= 0) {
            lList *list = lGetPosList(this_elem, pos);
            lListElem *elem = nullptr;
            lListElem *next_elem = nullptr;

            next_elem = lFirstRW(list);
            while ((elem = next_elem) != nullptr) {
               const char *attr_hostname = lGetHost(elem, 
                                 cqueue_attribute_array[index].href_attr);

               next_elem = lNextRW(elem);
               if (!sge_hostcmp(hgroup_or_hostname, attr_hostname)) {
                  lRemoveElem(list, &elem);
               }
            }
         }
         index++;
      }
   }
   DRETURN(ret);
}

/****** sge_cqueue/cqueue_purge_host() *****************************************
*  NAME
*     cqueue_purge_host() -- purge attributes from queue
*
*  SYNOPSIS
*     bool cqueue_purge_host(lListElem *this_elem, lList **answer_list, lList 
*     *attr_list, const char *hgroup_or_hostname) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     lListElem *this_elem           - cqueue list element 
*     lList **answer_list            - answer list 
*     lList *attr_list               - list with attributes to search for
*                                      hgroup_or_hostname      
*                                      wildcards allowed 
*     const char *hgroup_or_hostname -  host group or hostname which should be purged 
*
*  RESULT
*     bool -  
*        true     - hostname found and purged
*        false    - attribute or hostname not found    
*
*  NOTES
*     MT-NOTE: cqueue_purge_host() is not MT safe 
*
*******************************************************************************/
bool 
cqueue_purge_host(lListElem *this_elem, lList **answer_list, 
                  lList *attr_list, const char *hgroup_or_hostname)
{
   bool ret = false;
   int index;

   lList *sublist = nullptr;
   const lListElem *ep = nullptr;
   const char *attr_name = nullptr;

   DENTER(CQUEUE_LAYER);

   if (this_elem != nullptr) {
      for_each_ep(ep, attr_list) {
         attr_name = lGetString(ep, US_name);
         DPRINTF(SFQ "\n", attr_name);
      
         /* purge hostlist */         
         if (!sge_eval_expression(TYPE_HOST, attr_name, SGE_ATTR_HOSTLIST, nullptr)) {
            sublist = nullptr;
            lXchgList(this_elem, CQ_hostlist, &sublist);
            if (lDelElemHost(&sublist, HR_name, hgroup_or_hostname) == 1) {
               DPRINTF(SFQ " deleted in " SFQ "\n", hgroup_or_hostname, SGE_ATTR_HOSTLIST);
               ret = true;
            }
            lXchgList(this_elem, CQ_hostlist, &sublist);
         }
         
         /* purge attributes */ 
         index = 0;
         while(cqueue_attribute_array[index].name != nullptr) {

            /* Does the given attr_wildcard match with the actual attr_name */         
            if (!sge_eval_expression(TYPE_STR, attr_name, cqueue_attribute_array[index].name, nullptr)) {
               sublist = lGetListRW(this_elem, cqueue_attribute_array[index].cqueue_attr );

               if (lDelElemHost(&sublist, cqueue_attribute_array[index].href_attr, hgroup_or_hostname) == 1) {
                  DPRINTF(SFQ " deleted in " SFQ "\n", hgroup_or_hostname, cqueue_attribute_array[index].name);
                  ret = true;
               }
            }
            index++;
         }
      }
   }

   DRETURN(ret);
}

bool
cqueue_sick(lListElem *cqueue, lList **answer_list, 
            lList *master_hgroup_list, dstring *ds)
{
   bool ret = true;

   DENTER(TOP_LAYER);

   /*
    * Warn about:
    *    - unused setting for attributes
    *    - hgroup settings were not all hosts are contained in hostlist
    */
   {
      const char *cqueue_name = lGetString(cqueue, CQ_name);
      lList *used_hosts = nullptr;
      lList *used_groups = nullptr;
      lList **answer_list = nullptr;
      int index;
      
      /*
       * resolve href list of cqueue
       */
      href_list_find_all_references(lGetList(cqueue, CQ_hostlist), answer_list,
                                    master_hgroup_list, &used_hosts,
                                    &used_groups);

      index = 0;
      while (cqueue_attribute_array[index].cqueue_attr != NoName) {
         /*
          * Skip geee attributes in ge mode
          */
         lList *attr_list = lGetListRW(cqueue, cqueue_attribute_array[index].cqueue_attr);
         lListElem *next_attr = lFirstRW(attr_list);
         lListElem *attr = nullptr;

         /*
          * Test each attribute setting if it is really used in the
          * current configuration
          */
         while((attr = next_attr) != nullptr) {
            const char *name = lGetHost(attr, cqueue_attribute_array[index].href_attr);

            next_attr = lNextRW(attr);
            if (is_hgroup_name(name)) {
               if (strcmp(name, HOSTREF_DEFAULT)) {
                  lListElem *hgroup = nullptr;
                  lList *used_hgroup_hosts = nullptr;
                  lList *used_hgroup_groups = nullptr;
                  lList *add_hosts = nullptr;
                  lList *equity_hosts = nullptr;

                  hgroup = hgroup_list_locate(master_hgroup_list, name);

                  /*
                   * hgroup specific setting:
                   *    make sure each host of hgroup is part of 
                   *    resolved list
                   */
                  hgroup_find_all_references(hgroup, answer_list, 
                                             master_hgroup_list, 
                                             &used_hgroup_hosts,
                                             &used_hgroup_groups);
                  href_list_compare(used_hgroup_hosts, answer_list,
                                    used_hosts, &add_hosts, nullptr,
                                    &equity_hosts, nullptr);

                  if (lGetNumberOfElem(add_hosts)) {
                     DTRACE;
                     sge_dstring_sprintf_append(ds, 
                             MSG_CQUEUE_DEFOVERWRITTEN_SSSSS,
                             cqueue_attribute_array[index].name,
                             name, cqueue_name, name, cqueue_name);
                     sge_dstring_append(ds, "\n");
                  }

                  lFreeList(&add_hosts);
                  lFreeList(&equity_hosts);
                  lFreeList(&used_hgroup_hosts);
                  lFreeList(&used_hgroup_groups);
               } else {
                  DTRACE;
               }
            } else {
               /*
                * host specific setting:
                *    make sure the host is contained in resolved list 
                */ 
               if (!href_list_has_member(used_hosts, name)) {
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
      lFreeList(&used_hosts);
      lFreeList(&used_groups);
   }
   
   DRETURN(ret);
}

