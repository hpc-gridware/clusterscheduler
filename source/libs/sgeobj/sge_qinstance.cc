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
 *   Portions of this software are Copyright (c) 2011 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull_list.h"

#include "sgeobj/sge_manop.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_mesobj.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_path_alias.h"
#include "sgeobj/sge_eval_expression.h"
#include "sgeobj/cull/sge_resource_utilization_RDE_L.h"
#include "sgeobj/cull/sge_resource_utilization_RUE_L.h"
#include "sgeobj/msg_sgeobjlib.h"

#include "msg_common.h"
#include "uti/sge.h"

#define QINSTANCE_LAYER BASIS_LAYER

/****** sgeobj/qinstance/qinstance_list_locate() ******************************
*  NAME
*     qinstance_list_locate() -- find a qinstance 
*
*  SYNOPSIS
*     lListElem * 
*     qinstance_list_locate(const lList *this_list, 
*                           const char *hostname, const char *cqueue_name) 
*
*  FUNCTION
*     Find a qinstance in "this_list" which is part of the cluster queue
*     with the name "cqueue_name" and resides on the host with the name 
*     "hostname".
*
*  INPUTS
*     const lList *this_list  - QU_Type list
*     const char *hostname    - hostname 
*     const char *cqueue_name - cluster queue name 
*
*  RESULT
*     lListElem * - QU_Type element
*
*  NOTES
*     MT-NOTE: qinstance_list_locate() is MT safe 
*******************************************************************************/
lListElem *
qinstance_list_locate(const lList *this_list, const char *hostname,
                      const char *cqueue_name) {
   lListElem *ret = nullptr;

   if (cqueue_name == nullptr) {
      ret = lGetElemHostRW(this_list, QU_qhostname, hostname);
   } else {
      for_each_rw(ret, this_list) {
         const char *qname = lGetString(ret, QU_qname);
         const char *hname = lGetHost(ret, QU_qhostname);

         /* use qinstance expression */
         if (!sge_eval_expression(TYPE_CSTR, cqueue_name, qname, nullptr)) {
            if (!sge_eval_expression(TYPE_HOST, hostname, hname, nullptr)) {
               break;
            }
         }
      }
   }
   return ret;
}

/****** sgeobj/qinstance/qinstance_list_locate2() *****************************
*  NAME
*     qinstance_list_locate2() -- find a qinstance using the fullname 
*
*  SYNOPSIS
*     lListElem * 
*     qinstance_list_locate2(const lList *queue_list, 
*                            const char *full_name) 
*
*  FUNCTION
*     find a qinstance using the fullname 
*
*  INPUTS
*     const lList *queue_list - QU_Type list 
*     const char *full_name   - fullname of the qinstance (<cqueue>@<hostname>)
*
*  RESULT
*     lListElem * - QU_type element
*
*  NOTES
*     MT-NOTE: qinstance_list_locate2() is MT safe 
*******************************************************************************/
lListElem *
qinstance_list_locate2(const lList *queue_list, const char *full_name) {
   return lGetElemStrRW(queue_list, QU_full_name, full_name);
}

/****** sgeobj/qinstance/qinstance_get_name() *********************************
*  NAME
*     qinstance_get_name() -- returns the fullname of a qinstance object 
*
*  SYNOPSIS
*     const char * 
*     qinstance_get_name(const lListElem *this_elem, 
*                        dstring *string_buffer) 
*
*  FUNCTION
*     Returns the fullname of a qinstance object 
*
*  INPUTS
*     const lListElem *this_elem - QU_Type 
*     dstring *string_buffer     - dynamic string buffer 
*
*  RESULT
*     const char * - pointer to the internal string buffer of "string_buffer"
*
*  NOTES
*     MT-NOTE: qinstance_get_name() is MT safe 
*******************************************************************************/
const char *
qinstance_get_name(const lListElem *this_elem, dstring *string_buffer) {
   const char *ret = nullptr;

   if (this_elem != nullptr && string_buffer != nullptr) {
      ret = sge_dstring_sprintf(string_buffer, SFN "@" SFN,
                                lGetString(this_elem, QU_qname),
                                lGetHost(this_elem, QU_qhostname));
   }
   return ret;
}

/****** sgeobj/qinstance/qinstance_list_set_tag() *****************************
*  NAME
*     qinstance_list_set_tag() -- tag a list of qinstances 
*
*  SYNOPSIS
*     void 
*     qinstance_list_set_tag(lList *queue_list, u_long32 tag_value, int tag_nm = QU_tag)
*
*  FUNCTION
*     Tag a list of qinstances ("queue_list") with "tag_value".
*     Examples for tags are QU_tag (default) and QU_tagged4schedule.
*
*  INPUTS
*     lList *this_list   - QU_Type list
*     u_long32 tag_value - unsigned long value (not a bitmask)
*     int tag_nm         - which tag attribute to use (default: QU_tag)
*
*  RESULT
*     void - None
*
*  NOTES
*     MT-NOTE: qinstance_list_set_tag() is MT safe 
*******************************************************************************/
void qinstance_list_set_tag(lList *queue_list, u_long32 tag_value, int tag_nm) {
   if (queue_list != nullptr) {
      lListElem *qinstance;
      for_each_rw (qinstance, queue_list) {
         lSetUlong(qinstance, tag_nm, tag_value);
      }
   }
}

/****** sgeobj/qinstance/qinstance_increase_qversion() ************************
*  NAME
*     qinstance_increase_qversion() -- increase the qinstance queue version 
*
*  SYNOPSIS
*     void qinstance_increase_qversion(lListElem *this_elem) 
*
*  FUNCTION
*     Increase the queue version of the given qinstance "this_elem". 
*
*  INPUTS
*     lListElem *this_elem - QU_Type element 
*
*  RESULT
*     void - None
*
*  NOTES
*     MT-NOTE: qinstance_increase_qversion() is MT safe 
*******************************************************************************/
void
qinstance_increase_qversion(lListElem *this_elem) {
   DENTER(TOP_LAYER);
   lAddUlong(this_elem, QU_version, 1);
   DRETURN_VOID;
}

/****** sgeobj/qinstance/qinstance_check_owner() ******************************
*  NAME
*     qinstance_check_owner() -- check if a user is queue owner
*
*  SYNOPSIS
*     bool 
*     qinstance_check_owner(const lListElem *queue, const char *user_name) 
*
*  FUNCTION
*     Checks if the given user is an owner of the given queue.
*     Managers and operators are implicitly owner of all queues.
*
*  INPUTS
*     const lListElem *queue - the queue to check
*     const char *user_name  - the user name to check
*
*  RESULT
*     bool - true, if the user is owner, else false
******************************************************************************/
bool
qinstance_check_owner(const sge_gdi_packet_class_t *packet, const lListElem *this_elem,
                      const lList *master_manager_list, const lList *master_operator_list) {
   bool ret = false;

   DENTER(TOP_LAYER);
   if (packet == nullptr) {
      DRETURN(false);
   }
   if (this_elem == nullptr) {
      ret = false;
   } else if (manop_is_operator(packet, master_manager_list, master_operator_list)) {
      ret = true;
   } else {
      const lList *owner_list = lGetList(this_elem, QU_owner_list);
      if (lGetElemStr(owner_list, US_name, packet->user) != nullptr) {
         ret = true;
      }
   }
   DRETURN(ret);
}

/****** sgeobj/qinstance/qinstance_is_pe_referenced() *************************
*  NAME
*     qinstance_is_pe_referenced() -- Is the PE object referenced 
*
*  SYNOPSIS
*     bool 
*     qinstance_is_pe_referenced(const lListElem *this_elem, 
*                                const lListElem *pe) 
*
*  FUNCTION
*     Is the given PE ("pe") referenced in the qinstance element "this_elem". 
*
*  INPUTS
*     const lListElem *this_elem - QU_Type element 
*     const lListElem *pe        - PE_Type element 
*
*  RESULT
*     bool - test result 
*        true  - is referenced 
*        false - is not referenced 
*
*  NOTES
*     MT-NOTE: qinstance_is_pe_referenced() is MT safe 
*******************************************************************************/
bool
qinstance_is_pe_referenced(const lListElem *this_elem, const lListElem *pe) {
   bool ret = false;
   const lListElem *re_ref_elem;

   DENTER(TOP_LAYER);
   for_each_ep(re_ref_elem, lGetList(this_elem, QU_pe_list)) {
      if (pe_is_matching(pe, lGetString(re_ref_elem, ST_name))) {
         ret = true;
         break;
      }
   }
   DRETURN(ret);
}

/****** sgeobj/qinstance/qinstance_is_calendar_referenced() *******************
*  NAME
*     qinstance_is_calendar_referenced() -- is the calendar referenced 
*
*  SYNOPSIS
*     bool 
*     qinstance_is_calendar_referenced(const lListElem *this_elem, 
*                                      const lListElem *calendar) 
*
*  FUNCTION
*     Is the "calendar" referenced in the qinstance "this_elem". 
*
*  INPUTS
*     const lListElem *this_elem - QU_Type element 
*     const lListElem *calendar  - CAL_Type element 
*
*  RESULT
*     bool - test result 
*        true  - is referenced
*        false - is not referenced 
*
*  NOTES
*     MT-NOTE: qinstance_is_calendar_referenced() is MT safe 
*******************************************************************************/
bool
qinstance_is_calendar_referenced(const lListElem *this_elem,
                                 const lListElem *calendar) {
   bool ret = false;
   const char *queue_calendar = nullptr;

   DENTER(TOP_LAYER);
   queue_calendar = lGetString(this_elem, QU_calendar);
   if (queue_calendar != nullptr) {
      const char *calendar_name = lGetString(calendar, CAL_name);

      if (calendar_name && !strcmp(queue_calendar, calendar_name)) {
         ret = true;
      }
   }
   DRETURN(ret);
}

/****** sgeobj/qinstance/qinstance_is_a_pe_referenced() ***********************
*  NAME
*     qinstance_is_a_pe_referenced() -- is a PE referenced
*
*  SYNOPSIS
*     bool qinstance_is_a_pe_referenced(const lListElem *this_elem) 
*
*  FUNCTION
*     Test is at least one PE is referenced by qinstance "this_elem" 
*
*  INPUTS
*     const lListElem *this_elem - QU_Type 
*
*  RESULT
*     bool - test result
*        true  - an PE is referenced
*        false - no PE is referenced ("NONE")
*
*  NOTES
*     MT-NOTE: qinstance_is_a_pe_referenced() is MT safe 
*******************************************************************************/
bool
qinstance_is_a_pe_referenced(const lListElem *this_elem) {
   bool ret = false;

   DENTER(TOP_LAYER);
   if (lGetNumberOfElem(lGetList(this_elem, QU_pe_list))) {
      ret = true;
   }
   DRETURN(ret);
}

/****** sgeobj/qinstance/qinstance_is_ckpt_referenced() ***********************
*  NAME
*     qinstance_is_ckpt_referenced() -- Is the CKTP referenced 
*
*  SYNOPSIS
*     bool 
*     qinstance_is_ckpt_referenced(const lListElem *this_elem, 
*                                  const lListElem *ckpt) 
*
*  FUNCTION
*     Tests if the given CKPT object ("ckpt") is referenced in
*     the qinstance "this_elem". 
*
*  INPUTS
*     const lListElem *this_elem - QU_Type element
*     const lListElem *ckpt      - CKPT_Type element
*
*  RESULT
*     bool - test result
*        true  - CKPT is referenced
*        false - CKPT is not referenced
*
*  NOTES
*     MT-NOTE: qinstance_is_ckpt_referenced() is MT safe 
*******************************************************************************/
bool
qinstance_is_ckpt_referenced(const lListElem *this_elem, const lListElem *ckpt) {
   bool ret = false;
   const lList *ckpt_list = lGetList(this_elem, QU_ckpt_list);

   DENTER(TOP_LAYER);
   if (lGetElemStr(ckpt_list, ST_name, lGetString(ckpt, CK_name)) != nullptr) {
      ret = true;
   }
   DRETURN(ret);
}

/****** sgeobj/qinstance/qinstance_is_a_ckpt_referenced() *********************
*  NAME
*     qinstance_is_a_ckpt_referenced() -- Is an CKPT object referenced 
*
*  SYNOPSIS
*     bool qinstance_is_a_ckpt_referenced(const lListElem *this_elem) 
*
*  FUNCTION
*     Is an CKPT object referenced in "this_elem". 
*
*  INPUTS
*     const lListElem *this_elem - CKPT_Type element 
*
*  RESULT
*     bool - test result
*        true  - a CKPT is referenced
*        false - no CKPT is referenced
*
*  NOTES
*     MT-NOTE: qinstance_is_a_ckpt_referenced() is MT safe 
*******************************************************************************/
bool
qinstance_is_a_ckpt_referenced(const lListElem *this_elem) {
   bool ret = false;

   DENTER(TOP_LAYER);
   if (lGetNumberOfElem(lGetList(this_elem, QU_ckpt_list))) {
      ret = true;
   }
   DRETURN(ret);
}

/****** sgeobj/qinstance/qinstance_is_centry_a_complex_value() ****************
*  NAME
*     qinstance_is_centry_a_complex_value() -- Is it a complex_value 
*
*  SYNOPSIS
*     bool 
*     qinstance_is_centry_a_complex_value(const lListElem *this_elem, 
*                                         const lListElem *centry) 
*
*  FUNCTION
*     Is the given "centry" in the list of complex_values of "this_elem".
*
*  INPUTS
*     const lListElem *this_elem - QU_Type element 
*     const lListElem *centry    - CE_Type element 
*
*  RESULT
*     bool - test result
*        true  - it is a complex value
*        false - no complex value
*  NOTES
*     MT-NOTE: qinstance_is_centry_a_complex_value() is MT safe 
*******************************************************************************/
bool
qinstance_is_centry_a_complex_value(const lListElem *this_elem,
                                    const lListElem *centry) {
   bool ret = false;

   DENTER(TOP_LAYER);
   if (this_elem != nullptr) {
      const char *name = lGetString(centry, CE_name);
      const lList *centry_list = lGetList(this_elem, QU_consumable_config_list);
      const lListElem *centry_ref = lGetElemStr(centry_list, CE_name, name);

      if (centry_ref != nullptr || get_rsrc(name, true, nullptr, nullptr, nullptr, nullptr) == 0) {
         ret = true;
      }
   }
   DRETURN(ret);
}

/****** sgeobj/qinstance/qinstance_list_find_matching() ***********************
*  NAME
*     qinstance_list_find_matching() -- find certain qinstances 
*
*  SYNOPSIS
*     bool
*     qinstance_list_find_matching(const lList *this_list, 
*                                  lList **answer_list,
*                                  const char *hostname_pattern, 
*                                  lList **qref_list)
*
*  FUNCTION
*     Finds all qinstances in "this_list" whose hostname part matches
*     the "hostname_pattern" (fnmatch pattern) and stores the
*     qinstance name in "qref_list". In case of any error "answer_list"
*     will be filled.
*
*  INPUTS
*     const lList *this_list       - QU_Type list 
*     lList **answer_list          - AN_Type list 
*     const char *hostname_pattern - fnmatch hostname pattern 
*     lList **qref_list            - QR_Type list
*
*  RESULT
*     bool - error result
*        true  - success
*
*  NOTES
*     MT-NOTE: qinstance_list_find_matching() is MT safe 
*******************************************************************************/
bool
qinstance_list_find_matching(const lList *this_list, lList **answer_list,
                             const char *hostname_pattern, lList **qref_list) {
   bool ret = true;

   DENTER(QINSTANCE_LAYER);

   if (qref_list == nullptr) {
      DRETURN(true);
   }

   if (this_list != nullptr && hostname_pattern != nullptr) {
      const lListElem *qinstance;
      char host[CL_MAXHOSTLEN];

      if ((getuniquehostname(hostname_pattern, host, 0)) == CL_RETVAL_OK) {
         hostname_pattern = host;
      }

      for_each_ep(qinstance, this_list) {
         const char *hostname = lGetHost(qinstance, QU_qhostname);
         /* use qinstance expression */
         if (!sge_eval_expression(TYPE_HOST, hostname_pattern, hostname, answer_list)) {
            lAddElemStr(qref_list, QR_name, lGetString(qinstance, QU_full_name), QR_Type);
         }
      }
   }
   DRETURN(ret);
}

/****** sgeobj/qinstance/qinstance_slots_used() *******************************
*  NAME
*     qinstance_slots_used() -- Returns the number of currently used slots 
*
*  SYNOPSIS
*     int 
*     qinstance_slots_used(const lListElem *this_elem) 
*
*  FUNCTION
*     Returns the number of currently used slots. 
*
*  INPUTS
*     const lListElem *this_elem - QU_Type element 
*
*  RESULT
*     int - number of slots
*
*  NOTES
*     MT-NOTE: qinstance_slots_used() is MT safe 
*******************************************************************************/
int
qinstance_slots_used(const lListElem *this_elem) 
{
   DENTER(QINSTANCE_LAYER);
   const lListElem *slots = lGetSubStr(this_elem, RUE_name, SGE_ATTR_SLOTS, QU_resource_utilization);
   int ret = 1000000; /* when slots is unknown */

   if (slots != nullptr) {
      ret = lGetDouble(slots, RUE_utilized_now);
   } else {
      /* this happens on qinstance_create when a queue instance is created 
         before others queue instances in the subordinate lists  
         are created */
      CRITICAL(MSG_QINSTANCE_MISSLOTS_S, lGetString(this_elem, QU_full_name));
   }
   DRETURN(ret);
}

/****** sgeobj/qinstance/qinstance_slots_reserved() ***************************
*  NAME
*     qinstance_slots_reserved() -- the number of maximal reserved slots
*
*  SYNOPSIS
*     int qinstance_slots_reserved(const lListElem *this_elem) 
*
*  FUNCTION
*     Returns the number of maximal reserved slots by all advance reservations
*
*  INPUTS
*     const lListElem *this_elem - QU_Type element
*
*  RESULT
*     int - number of slots
*
*  NOTES
*     MT-NOTE: qinstance_slots_reserved() is MT safe 
*******************************************************************************/
u_long32
qinstance_slots_reserved(const lListElem *this_elem) {
   u_long32 ret = 0;
   const lListElem *slots;
   const lListElem *utilized;

   DENTER(QINSTANCE_LAYER);

   slots = lGetSubStr(this_elem, RUE_name, SGE_ATTR_SLOTS, QU_resource_utilization);
   if (slots != nullptr) {
      for_each_ep(utilized, lGetList(slots, RUE_utilized)) {
         ret = MAX(ret, lGetDouble(utilized, RDE_amount));
      }
   }

   DRETURN(ret);
}

/****** sgeobj/qinstance/qinstance_set_slots_used() ***************************
*  NAME
*     qinstance_set_slots_used() -- Modifies the number of used slots 
*
*  SYNOPSIS
*     void 
*     qinstance_set_slots_used(lListElem *this_elem, int new_slots) 
*
*  FUNCTION
*     Modifies the number of used slots 
*
*  INPUTS
*     lListElem *this_elem - QU_Type 
*     int new_slots        - new slot value 
*
*  RESULT
*     void - NONE 
*
*  NOTES
*     MT-NOTE: qinstance_set_slots_used() is MT safe 
*******************************************************************************/
void
qinstance_set_slots_used(lListElem *this_elem, int new_slots) {
   DENTER(QINSTANCE_LAYER);
   lListElem *slots = lGetSubStrRW(this_elem, RUE_name, "slots", QU_resource_utilization);
   if (slots != nullptr) {
      lSetDouble(slots, RUE_utilized_now, new_slots);
   } else {
      /* because this should never happen and an critical error */
      CRITICAL(MSG_QINSTANCE_MISSLOTS_S, lGetString(this_elem, QU_full_name));
   }
   DRETURN_VOID;
}

/****** sgeobj/qinstance/qinstance_debit_consumable() *************************
*  NAME
*     qinstance_debit_consumable() -- Debits/Undebits consumables
*
*  SYNOPSIS
*     int
*     qinstance_debit_consumable(lListElem *qep, 
*                                const lListElem *jep,
*                                lList *centry_list,
*                                int slots)
*
*  FUNCTION
*     Checks if there are nonstatic load values avaialable for the
*     qinstance. If this is the case, then then the "unknown" state 
*     of that machine will be released. 
*
*  INPUTS
*     lListElem *qep     - Qinstance resource container
*     lListElem *jep     - The job (JB_Type) defining which resources and how
*                          much of them need to be (un)debited
*     lList *centry_list - The global complex list that is needed to interpret
*                          the jobs' resource requests.
*     int slots          - The number of slots for which we are debiting.
*                          Positive slots numbers cause debiting, negative
*                          ones cause undebiting.
*
*  RESULT
*     Returns -1 in case of an error. Otherwise the number of (un)debitations 
*     that actually took place is returned. If 0 is returned that means the
*     consumable resources of the 'ep' object has not changed.
*
*  NOTES
*     MT-NOTE: qinstance_debit_consumable() is MT safe 
*******************************************************************************/
int
qinstance_debit_consumable(lListElem *qep, const lListElem *jep, const lListElem *pe, const lList *centry_list,
                           int slots, bool is_master_task, bool do_per_host_booking, bool *just_check) {
   return rc_debit_consumable(jep, pe, qep, centry_list, slots,
                              QU_consumable_config_list,
                              QU_resource_utilization,
                              lGetString(qep, QU_qname), is_master_task, do_per_host_booking, just_check);
}

/****** sgeobj/qinstance/qinstance_message_add() *****************************
*  NAME
*     qinstance_message_add() -- Adds a message to the qinstance structure 
*
*  SYNOPSIS
*     bool
*     qinstance_message_add(lListElem *this_elem, u_long32 type, 
*                           const char *message)
*
*  FUNCTION
*     Adds a message to the qinstance structure
*
*  INPUTS
*     lListElem *this_elem - QU_Type 
*     u_long32 type        - message type
*     const char *message  - message
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error 
*
*  NOTES
*     MT-NOTE: qinstance_message_add() is MT safe 
*******************************************************************************/
bool
qinstance_message_add(lListElem *this_elem, u_long32 type, const char *message) {
   bool ret = true;

   DENTER(TOP_LAYER);
   object_message_add(this_elem, QU_message_list, type, message);
   DRETURN(ret);
}

/****** sgeobj/qinstance/qinstance_message_trash_all_of_type_X() **************
*  NAME
*     qinstance_message_trash_all_of_type_X() -- Trash messages 
*
*  SYNOPSIS
*     bool
*     qinstance_message_trash_all_of_type_X(lListElem *this_elem, 
*                                           u_long32 type)
*
*  FUNCTION
*     Removes all messages with the message "type" id. 
*
*  INPUTS
*     lListElem *this_elem - QU_Type 
*     u_long32 type        - message type
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error 
*
*  NOTES
*     MT-NOTE: qinstance_message_trash_all_of_type_X() is MT safe 
*******************************************************************************/
bool
qinstance_message_trash_all_of_type_X(lListElem *this_elem, u_long32 type) {
   bool ret = true;

   DENTER(TOP_LAYER);
   object_message_trash_all_of_type_X(this_elem, QU_message_list, type);
   DRETURN(ret);
}

/****** sgeobj/qinstance/qinstance_set_full_name() ****************************
*  NAME
*     qinstance_set_full_name() -- set the full name of the qinstance 
*
*  SYNOPSIS
*     void
*     qinstance_set_full_name(lListElem *this_elem)
*
*  FUNCTION
*     Set the full name of the qinstance. The QU_qname name attribute
*     will be used as input for the cqueue part and QU_qhostname
*     as hostname part of the full name.
*
*  INPUTS
*     lListElem *this_elem - QU_Type 
*
*  RESULT
*     void - NONE
*
*  NOTES
*     MT-NOTE: qinstance_set_full_name() is MT safe 
*******************************************************************************/
void
qinstance_set_full_name(lListElem *this_elem) {
   dstring buffer = DSTRING_INIT;
   const char *cqueue_name = lGetString(this_elem, QU_qname);
   const char *hostname = lGetHost(this_elem, QU_qhostname);

   sge_dstring_sprintf(&buffer, "%s@%s", cqueue_name, hostname);
   lSetString(this_elem, QU_full_name,
              sge_dstring_get_string(&buffer));
   sge_dstring_free(&buffer);
}

/****** sgeobj/qinstance/qinstance_validate() *********************************
*  NAME
*     qinstance_validate() -- validates and initializes qinstances 
*
*  SYNOPSIS
*     bool
*     qinstance_validate(lListElem *this_elem, lList **answer_list)
*
*  FUNCTION
*     Validates qinstance attributes and initializes them if necessary.
*
*  INPUTS
*     lListElem *this_elem - QU_Type 
*     lList **answer_list - AN_Type
*
*  RESULT
*     void - error result
*        true  - success
*        false - error 
*
*  NOTES
*     MT-NOTE: qinstance_validate() is MT safe 
*******************************************************************************/
bool
qinstance_validate(lListElem *this_elem, lList **answer_list, const lList *master_exechost_list,
                   const lList *master_centry_list) {
   bool ret = true;

   DENTER(TOP_LAYER);

   /* QU_full_name isn't spooled, if it is not set, create it */
   if (lGetString(this_elem, QU_full_name) == nullptr) {
      qinstance_set_full_name(this_elem);
   }

   /* handle slots from now on as a consumble attribute of queue */
   qinstance_set_conf_slots_used(this_elem);

   /* remove all queue message, which are regenerated during the unspooling
      the queue */
   qinstance_message_trash_all_of_type_X(this_elem, ~QI_ERROR);

   /* setup actual list of queue */
   qinstance_debit_consumable(this_elem, nullptr, nullptr, master_centry_list, 0, true, true, nullptr);

   /* init double values of consumable configuration */
   if (centry_list_fill_request(lGetListRW(this_elem, QU_consumable_config_list), answer_list, master_centry_list, true,
                                false, true) != 0) {
      ret = false;
   }

   if (ret) {
      if (ensure_attrib_available(nullptr, this_elem, QU_load_thresholds, master_centry_list) ||
          ensure_attrib_available(nullptr, this_elem, QU_suspend_thresholds, master_centry_list) ||
          ensure_attrib_available(nullptr, this_elem, QU_consumable_config_list, master_centry_list)) {
         ret = false;
      }
   }

   /* qinstance state */
   if (ret) {
      qinstance_state_set_unknown(this_elem, true);
      qinstance_state_set_cal_disabled(this_elem, false);
      qinstance_state_set_cal_suspended(this_elem, false);
      qinstance_set_slots_used(this_elem, 0);

      if (host_list_locate(master_exechost_list,
                           lGetHost(this_elem, QU_qhostname)) == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                 ANSWER_QUALITY_ERROR,
                                 MSG_QINSTANCE_HOSTFORQUEUEDOESNOTEXIST_SS,
                                 lGetString(this_elem, QU_qname),
                                 lGetHost(this_elem, QU_qhostname));
         ret = false;
      }
   }

   DRETURN(ret);
}

/****** sgeobj/qinstance/qinstance_list_validate() ****************************
*  NAME
*     qinstance_list_validate() -- validates and initializes qinstances 
*
*  SYNOPSIS
*     bool
*     qinstance_list_validate(lList *this_list, lList **answer_list)
*
*  FUNCTION
*     Validates qinstances attributes and initializes them if necessary.
*
*  INPUTS
*     lList *this_list - QU_Type list
*     lList **answer_list - AN_Type
*
*  RESULT
*     void - error result
*        true  - success
*        false - error 
*
*  NOTES
*     MT-NOTE: qinstance_list_validate() is MT safe 
*******************************************************************************/
bool
qinstance_list_validate(lList *this_list, lList **answer_list, const lList *master_exechost_list,
                        const lList *master_centry_list) {
   bool ret = true;
   lListElem *qinstance;

   DENTER(TOP_LAYER);

   for_each_rw(qinstance, this_list) {
      if (!qinstance_validate(qinstance, answer_list, master_exechost_list, master_centry_list)) {
         ret = false;
         break;
      }
   }

   DRETURN(ret);
}

// returns true, false in case of just_check and the check fails
static bool
rc_debit_consumable_implicit_exclusive(const char *name, const char *obj_type, const char *obj_name, int slots,
                                       lListElem *booking_ep, bool *just_check) {
   DENTER(TOP_LAYER);
   bool ret = true;

   /*
    * The job doesn't request the exclusive complex, but it still has effect:
    * A job actually requesting to run exclusive will not be dispatched
    * to this host - scheduler checks if there is a job running either
    * requesting the exclusive resource or a job not requesting it is
    * blocking the host.
    *
    * For the consumable check (just_check != nullptr) this means that
    * we check if the resource is actually in use (RU_utilized_now).
    */
   double request = 1.0;
   if (just_check == nullptr) {
      DPRINTF("debiting (implicit exclusive) %f of %s on %s %s for %d slots\n", request, name,
              obj_type, obj_name, slots);
      lAddDouble(booking_ep, RUE_utilized_now_nonexclusive, slots * request);
   } else {
      double actual_value = booking_ep == nullptr ? 0 : lGetDouble(booking_ep, RUE_utilized_now);
      if (actual_value > 0) {
         ERROR(MSG_EXCLCAPACITYEXCEEDED_FSSSI, request, name, obj_type, obj_name, slots);
         *just_check = false;
         ret = false;
      }
   }

   DRETURN(ret);
}

static bool
rc_debit_consumable_explicit_request(const char *name, const char *obj_type, const char *obj_name, int slots,
                                     double request, double additional_request, const lListElem *capacity_ep,
                                     lListElem *booking_ep, bool is_exclusive, bool *just_check) {
   DENTER(TOP_LAYER);
   bool ret = true;

   if (just_check == nullptr) {
      DPRINTF("debiting %f of %s on %s %s for %d slots\n", request, name, obj_type, obj_name, slots);
      lAddDouble(booking_ep, RUE_utilized_now, slots * request);
   } else {
      DPRINTF("verifying %f of %s on %s %s for %d slots + %f\n", request, name, obj_type, obj_name, slots, additional_request);
      double actual_value = booking_ep == nullptr ? 0 : lGetDouble(booking_ep, RUE_utilized_now);
      double config_value = lGetDouble(capacity_ep, CE_doubleval);
      /* for exclusive consumables ignore the number of slots */
      if (is_exclusive) {
         slots = 1;
      }
      if ((config_value - actual_value - slots * request - additional_request) < 0) {
         ERROR(MSG_CAPACITYEXCEEDED_FFSSSIF, request, additional_request, name, obj_type, obj_name, slots, config_value - actual_value);
         *just_check = false;
         ret = false;
      }
   }

   DRETURN(ret);
}

/****** lib/sgeobj/debit_consumable() ****************************************
*  NAME
*     rc_debit_consumable() -- Debit/Undebit consumables from resource container
*
*  SYNOPSIS
*     int 
*     rc_debit_consumable(lListElem *jep, lListElem *ep, lList *centry_list, 
*                         int slots, int config_nm, int actual_nm, 
*                         const char *obj_name)
*
*  FUNCTION
*     Updates all consumable actual values of a resource container
*     for 'slots' slots of the given job. Positive slots numbers 
*     cause debiting, negative ones cause undebiting.
*
*  INPUTS
*     lListElem *jep       - The job (JB_Type) defining which resources and how
*                            much of them need to be (un)debited
*                            
*     lListElem *ep        - The resource container (global/host/queue) 
*                            that owns the resources (EH_Type).
* 
*     lList *centry_list   - The global complex list that is needed to interpret
*                            the jobs' resource requests.
*
*     int slots            - The number of slots for which we are debiting.
*                            Positive slots numbers cause debiting, negative 
*                            ones cause undebiting.
*
*     int config_nm        - The CULL field of the 'ep' object that contains a
*                            CE_Type list of configured complex values.
* 
*     int actual_nm        - The CULL field of the 'ep' object that contains a
*                            CE_Type list of actual complex values.
*
*     const char *obj_name - The name of the object we are debiting from. This
*                            is only used for monitoring/diagnosis purposes.
*
*     bool *just_check     - If != nullptr do not do the actual debiting, but just
*                            check if debiting would exceed resources.
*                            Only makes sense for debiting (slots > 0).
*
*  RESULT
*     Returns -1 in case of an error. Otherwise the number of (un)debitations
*     that actually took place is returned. If 0 is returned that means the
*     consumable resources of the 'ep' object has not changed.
******************************************************************************/
int
rc_debit_consumable(const lListElem *jep, const lListElem *pe, lListElem *ep, const lList *centry_list, int slots,
                    int config_nm, int actual_nm, const char *obj_name, bool is_master_task,
                    bool do_per_host_booking, bool *just_check) {
   DENTER(TOP_LAYER);

   int mods = 0;

   if (ep == nullptr) {
      DRETURN(0);
   }

   /* assume debiting would work */
   if (just_check != nullptr) {
      *just_check = true;
   }

   // loop over all queue/exechost complex_values (QU_consumable_config_list, EH_consumable_config_list)
   const lListElem *cr_config;
   for_each_ep(cr_config, lGetList(ep, config_nm)) {
      lListElem *cr = nullptr, *dcep;
      const char *name = lGetString(cr_config, CE_name);

      // search complex definition (with default request)
      if (!(dcep = centry_list_locate(centry_list, name))) {
         ERROR(MSG_ATTRIB_MISSINGATTRIBUTEXINCOMPLEXES_S , name);
         DRETURN(-1);
      }

      u_long32 consumable = lGetUlong(dcep, CE_consumable);

      // ensure attribute is in actual list
      // @todo we could do this only if jep == nullptr - this is the call to initialize booking
      if (consumable != CONSUMABLE_NO) {
         cr = lGetSubStrRW(ep, RUE_name, name, actual_nm);
         if (just_check == nullptr && cr == nullptr) {
            cr = lAddSubStr(ep, RUE_name, name, actual_nm, RUE_Type);
            /* RUE_utilized_now is implicitly set to zero */
         }
      }

      if (jep != nullptr) {
         if (!consumable_do_booking(consumable, is_master_task, do_per_host_booking, false)) { // todo: remove last param, no longer used
            continue;
         }

         bool did_booking = false;
         bool is_exclusive = lGetUlong(dcep, CE_relop) == CMPLXEXCL_OP;
         const char *obj_type = config_nm == QU_consumable_config_list ? "queue" : "host";

         int debit_slots = consumable_get_debit_slots(consumable, slots);
         DPRINTF("===> rc_debit_consumable(): %s: %s: debit_slots: %d\n", obj_name, name, debit_slots);

         // has contribution from global requests? Then we can do the booking for master and slave task in one step.
         double dval = 0.0;
         bool tmp_ret = job_get_contribution_by_scope(jep, nullptr, name, &dval, dcep, JRS_SCOPE_GLOBAL);
         if (tmp_ret) {
            // the resource was requested
            DPRINTF("===> rc_debit_consumable(): %s: we have GLOBAL %s request: %f for %d slots\n", obj_name, name, dval, debit_slots);
            if (dval != 0.0) {
               if (!rc_debit_consumable_explicit_request(name, obj_type, obj_name, debit_slots, dval, 0.0, cr_config, cr,
                                                         is_exclusive, just_check)) {
                  break;
               }
               mods++;
               did_booking = true;
            }
         } else if (pe != nullptr) {
            // no global contribution, need to check master and slave
            // we use the original slots value for slave_debit_slots
            // reason: for host consumables, debit_slots is 1, which will be adjusted below to 0
            //         if we have then a slave request for a host consumable, it will not be booked!
            int slave_debit_slots = slots;
            double master_dval = 0.0;
            if (is_master_task) {
               // if the master task is part of this booking, check if we have a master request
               tmp_ret = job_get_contribution_by_scope(jep, nullptr, name, &master_dval, dcep, JRS_SCOPE_MASTER);
               if (tmp_ret) {
                  // we have a master request
                  DPRINTF("===> rc_debit_consumable(): %s: we have MASTER %s request: %f for %d slots\n", obj_name, name, master_dval,
                          slot_signum(debit_slots));
                  if (master_dval != 0.0) {
                     // book it for one slot
                     if (!rc_debit_consumable_explicit_request(name, obj_type, obj_name, slot_signum(debit_slots),
                                                               master_dval, 0.0, cr_config, cr, is_exclusive, just_check)) {
                        break;
                     }
                     mods++;
                     did_booking = true;
                  }
               }

               // if we did the master task booking
               // adjust the slot count for the slave booking
               adjust_slave_task_debit_slots(pe, slave_debit_slots);
            }

            // now do booking for the (remaining) slave tasks, if any
            if (slave_debit_slots != 0) {
               double slave_dval = 0.0;
               tmp_ret = job_get_contribution_by_scope(jep, nullptr, name, &slave_dval, dcep, JRS_SCOPE_SLAVE);
               if (tmp_ret && slave_dval != 0.0) {
                  // we have a slave request
                  slave_debit_slots = consumable_get_debit_slots(consumable, slave_debit_slots);
                  DPRINTF("===> rc_debit_consumable(): %s: we have SLAVE %s request: %f for %d slots\n", obj_name, name, slave_dval, slave_debit_slots);
                  if (slave_dval != 0.0) {
                     // in case of just_check: need to pass the master_dval as additional usage on this host/queue
                     if (!rc_debit_consumable_explicit_request(name, obj_type, obj_name, slave_debit_slots,
                                                               slave_dval, just_check != nullptr ? master_dval : 0.0,
                                                               cr_config, cr, is_exclusive, just_check)) {
                        break;
                     }
                     mods++;
                     did_booking = true;
                  }
               }
            }
         }

         // We didn't have any explicit request for this variable, but it is an exclusive - do implicit booking
         if (!did_booking && is_exclusive) {
            if (!rc_debit_consumable_implicit_exclusive(name, obj_type, obj_name, debit_slots, cr, just_check)) {
               break;
            }
            mods++;
         }
      }
   }

   DRETURN(mods);
}

void 
qinstance_set_conf_slots_used(lListElem *this_elem) {
   DENTER(QINSTANCE_LAYER);
   lListElem *slots = lGetSubStrRW(this_elem, CE_name, "slots", QU_consumable_config_list);
   if (slots == nullptr) {
      slots = lAddSubStr(this_elem, CE_name, "slots", QU_consumable_config_list, CE_Type);
   }
   if (slots != nullptr) {
      dstring buffer = DSTRING_INIT;
      u_long32 slots_value = lGetUlong(this_elem, QU_job_slots);

      sge_dstring_sprintf(&buffer, sge_u32, slots_value);
      lSetDouble(slots, CE_doubleval, slots_value);
      lSetString(slots, CE_stringval, sge_dstring_get_string(&buffer));
      sge_dstring_free(&buffer);
   }
   DRETURN_VOID;
}

/****** sge_qinstance/qinstance_list_verify_execd_job() ************************
*  NAME
*     qinstance_list_verify_execd_job() -- verify a queue instance list
*
*  SYNOPSIS
*     bool 
*     qinstance_list_verify_execd_job(const lList *queue_list, lList **answer_list) 
*
*  FUNCTION
*     Verify correctness of a queue instance list, that has been sent by qmaster
*     to execd as part of a job start order.
*
*  INPUTS
*     const lList *queue_list - the queue instance list
*     lList **answer_list     - answer list to pass back error messages
*
*  RESULT
*     bool - true: everything ok, else false
*
*  NOTES
*     MT-NOTE: qinstance_list_verify_execd_job() is MT safe 
*
*  SEE ALSO
*     sge_qinstance/qinstance_verify()
*******************************************************************************/
bool
qinstance_list_verify_execd_job(const lList *queue_list, lList **answer_list) {

   DENTER(TOP_LAYER);

   if (queue_list == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_NULLELEMENTPASSEDTO_S, __func__);
      DRETURN(false);
   } else {
      const lListElem *qep;

      for_each_ep(qep, queue_list) {
         if (!qinstance_verify(qep, answer_list)) {
            DRETURN(false);
         }
      }
   }

   DRETURN(true);
}

/****** sge_qinstance/qinstance_verify() ***************************************
*  NAME
*     qinstance_verify() -- verify a queue instance in execd
*
*  SYNOPSIS
*     bool 
*     qinstance_verify(const lListElem *qep, lList **answer_list) 
*
*  FUNCTION
*     Verify a single queue instance, that has been sent by qmaster to execd
*     as part of a job start order.
*
*  INPUTS
*     const lListElem *qep - the queue instance to verify
*     lList **answer_list  - answer list to pass back error messages
*
*  RESULT
*     bool - true: everything ok, else false
*
*  NOTES
*     MT-NOTE: qinstance_verify() is MT safe 
*******************************************************************************/
bool
qinstance_verify(const lListElem *qep, lList **answer_list) {
   bool ret = true;

   DENTER(TOP_LAYER);

   if (qep == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_NULLELEMENTPASSEDTO_S, __func__);
      ret = false;
   }

   if (ret) {
      ret = verify_host_name(answer_list, lGetHost(qep, QU_qhostname));
   }

   if (ret) {
      if (verify_str_key(answer_list, lGetString(qep, QU_qname),
                         MAX_VERIFY_STRING, lNm2Str(QU_qname), KEY_TABLE) != STATUS_OK) {
         ret = false;
      }
   }

   if (ret) {
      ret = qinstance_verify_full_name(answer_list, lGetString(qep, QU_full_name));
   }

   if (ret) {
      ret = path_verify(lGetString(qep, QU_shell), answer_list, "shell", true);
   }

   DRETURN(ret);
}

/****** sge_qinstance/qinstance_verify_full_name() *****************************
*  NAME
*     qinstance_verify_full_name() -- verify a queue instance full name
*
*  SYNOPSIS
*     bool 
*     qinstance_verify_full_name(lList **answer_list, const char *full_name) 
*
*  FUNCTION
*     Verifies, if a queue instance full name is correct (form cqueue@host).
*
*  INPUTS
*     lList **answer_list   - answer list to pass back error messages
*     const char *full_name - the queue instance name to verify
*
*  RESULT
*     bool - true: everything ok, else false
*
*  NOTES
*     MT-NOTE: qinstance_verify_full_name() is MT safe 
*******************************************************************************/
bool
qinstance_verify_full_name(lList **answer_list, const char *full_name) {
   bool ret = true;

   dstring cqueue_name = DSTRING_INIT;
   dstring host_domain = DSTRING_INIT;
   bool has_hostname, has_domain;

   if (full_name == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_INVALID_QINSTANCE_NAME_S, "<null>");
      ret = false;
   }

   /* split the queue instance name and verify its components */
   if (ret) {
      if (!cqueue_name_split(full_name, &cqueue_name, &host_domain,
                             &has_hostname, &has_domain)) {
         /* report syntax error */
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_INVALID_QINSTANCE_NAME_S, full_name);
         ret = false;
      }
   }

   /* the cqueue name */
   if (ret) {
      if (verify_str_key(answer_list, sge_dstring_get_string(&cqueue_name),
                         MAX_VERIFY_STRING, "cluster queue", KEY_TABLE) != STATUS_OK) {
         ret = false;
      }
   }

   /* the hostname or host group */
   if (ret) {
      if (has_hostname) {
         ret = verify_host_name(answer_list, sge_dstring_get_string(&host_domain));
      } else if (has_domain) {
         if (verify_str_key(answer_list, sge_dstring_get_string(&host_domain) + 1,
                            MAX_VERIFY_STRING, "host domain", KEY_TABLE) != STATUS_OK) {
            ret = false;
         }
      } else {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_INVALID_QINSTANCE_NAME_S, full_name);
         ret = false;
      }
   }

   sge_dstring_free(&cqueue_name);
   sge_dstring_free(&host_domain);

   return ret;
}

/****** sge_qinstance/qinstance_set_error() ************************************
*  NAME
*     qinstance_set_error() -- set/unset qinstance into state
*
*  SYNOPSIS
*     void qinstance_set_error(lListElem *qinstance, u_long32 type, const char 
*     *message, bool set_error) 
*
*  FUNCTION
*     Sets or Unsets a qinstance into error state and adds or removes the given
*     error message
*
*  INPUTS
*     lListElem *qinstance - qinstance object (QU_Type)
*     u_long32 type        - new state
*     const char *message  - error message to set
*     bool set_error       - set or unset
*
*  NOTES
*     MT-NOTE: qinstance_set_error() is MT safe 
*******************************************************************************/
void
qinstance_set_error(lListElem *qinstance, u_long32 type, const char *message, bool set_error) {
   qinstance_set_state(qinstance, set_error, type);
   if (set_error) {
      qinstance_message_add(qinstance, type, message);
   } else {
      qinstance_message_trash_all_of_type_X(qinstance, type);
   }
}
