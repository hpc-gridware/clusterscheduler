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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2011 Univa Corporation
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstring>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/msg_sgeobjlib.h"

#include "sched/valid_queue_user.h"

#include "spool/sge_spooling.h"

#include "sge.h"
#include "sge_c_gdi.h"
#include "sge_utility_qmaster.h"
#include "sge_cqueue_qmaster.h"
#include "sge_qinstance_qmaster.h"
#include "sge_host_qmaster.h"
#include "sge_qmod_qmaster.h"
#include "sge_subordinate_qmaster.h"
#include "evm/sge_queue_event_master.h"
#include "evm/sge_event_master.h"
#include "sge_persistence_qmaster.h"
#include "sge_userprj_qmaster.h"
#include "sge_userset_qmaster.h"
#include "msg_common.h"
#include "msg_qmaster.h"


static bool
cqueue_mod_hostlist(lListElem *cqueue, lList **answer_list, lListElem *reduced_elem, int sub_command, lList **add_hosts,
                    lList **rem_hosts);

static bool
cqueue_mod_attributes(lListElem *cqueue, lList **answer_list, lListElem *reduced_elem, int sub_command);

static bool
cqueue_mark_qinstances(lListElem *cqueue, lList **answer_list, lList *del_hosts);

static bool
cqueue_add_qinstances(lListElem *cqueue, lList **answer_list, lList *add_hosts,
                      monitoring_t *monitor, const lList *master_hgroup_list, lList *master_cqueue_list);

static lListElem *
qinstance_create(const lListElem *cqueue, lList **answer_list, const char *hostname,
                 bool *is_ambiguous, monitoring_t *monitor, const lList *master_hgroup_list, lList *master_cqueue_list);

static void
cqueue_update_categories(const lListElem *new_cq, const lListElem *old_cq);

static void
qinstance_check_unknown_state(lListElem *this_elem, const lList *master_exechost_list);

static lListElem *
qinstance_create(const lListElem *cqueue, lList **answer_list, const char *hostname,
                 bool *is_ambiguous, monitoring_t *monitor, const lList *master_hgroup_list,
                 lList *master_cqueue_list) {
   DENTER(TOP_LAYER);
   dstring buffer = DSTRING_INIT;
   const char *cqueue_name = lGetString(cqueue, CQ_name);
   const lList *centry_list = *object_type_get_master_list(SGE_TYPE_CENTRY);
   const lList *master_ehost_list = *object_type_get_master_list(SGE_TYPE_EXECHOST);
   int index;
   lListElem *ret = lCreateElem(QU_Type);

   /*
    * Pre-initialize some fields: hostname, full_name
    */
   lSetHost(ret, QU_qhostname, hostname);
   lSetString(ret, QU_qname, cqueue_name);
   sge_dstring_sprintf(&buffer, "%s@%s", cqueue_name, hostname);
   lSetString(ret, QU_full_name, sge_dstring_get_string(&buffer));
   sge_dstring_free(&buffer);

   /*
    * Initialize configuration attributes from CQ
    */
   *is_ambiguous = false;
   index = 0;
   while (cqueue_attribute_array[index].cqueue_attr != NoName) {
      bool tmp_is_ambiguous = false;
      bool tmp_has_changed_conf_attr = false;
      bool tmp_has_changed_state_attr = false;
      const char *matching_host_or_group = nullptr;
      const char *matching_group = nullptr;

      qinstance_modify_attribute(ret, answer_list, cqueue,
                                 cqueue_attribute_array[index].qinstance_attr,
                                 cqueue_attribute_array[index].cqueue_attr,
                                 cqueue_attribute_array[index].href_attr,
                                 cqueue_attribute_array[index].value_attr,
                                 cqueue_attribute_array[index].primary_key_attr,
                                 &matching_host_or_group,
                                 &matching_group,
                                 &tmp_is_ambiguous,
                                 &tmp_has_changed_conf_attr,
                                 &tmp_has_changed_state_attr,
                                 true, nullptr, monitor, master_hgroup_list, master_cqueue_list);

      *is_ambiguous |= tmp_is_ambiguous;

      index++;
   }

   qinstance_set_conf_slots_used(ret);
   qinstance_debit_consumable(ret, nullptr, centry_list, 0, true, nullptr);

   /*
    * Change qinstance state
    */
   sge_qmaster_qinstance_state_set_ambiguous(ret, *is_ambiguous);
   if (*is_ambiguous) {
      DPRINTF(("Qinstance "SFN"@"SFN" has ambiguous configuration\n",
              cqueue_name, hostname));
   } else {
      DPRINTF(("Qinstance "SFN"@"SFN" has non-ambiguous configuration\n",
              cqueue_name, hostname));
   }

   /*
    * For new qinstances we have to set some internal fields which
    * will be spooled later on:
    *    - state (modification according to initial state)
    *    - qversion
    */
   sge_qmaster_qinstance_state_set_unknown(ret, true);
   qinstance_check_unknown_state(ret, master_ehost_list);
   sge_qmaster_qinstance_set_initial_state(ret);
   qinstance_initialize_sos_attr(ret, monitor, master_cqueue_list);

   qinstance_increase_qversion(ret);

   DRETURN(ret);
}

static bool
cqueue_add_qinstances(lListElem *cqueue, lList **answer_list, lList *add_hosts,
                      monitoring_t *monitor, const lList *master_hgroup_list, lList *master_cqueue_list) {
   bool ret = true;

   DENTER(TOP_LAYER);
   if (cqueue != nullptr && add_hosts != nullptr) {
      const lListElem *href;

      for_each_ep(href, add_hosts) {
         const char *hostname = lGetHost(href, HR_name);
         lList *list = lGetListRW(cqueue, CQ_qinstances);
         lListElem *qinstance = lGetElemHostRW(list, QU_qhostname, hostname);

         if (qinstance != nullptr) {
            if (qinstance_state_is_orphaned(qinstance)) {
               sge_qmaster_qinstance_state_set_orphaned(qinstance, false);
               lSetUlong(qinstance, QU_tag, SGE_QI_TAG_MOD);
            } else {
               /*
                * We might already have this QI if it is in orphaned state.
                * If this is not true, than there is a bug!
                */
               ERROR((SGE_EVENT, MSG_QINSTANCE_QIALREADYHERE_S, hostname));
               answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX,
                               ANSWER_QUALITY_ERROR);
            }
         } else {
            bool is_ambiguous = false;

            if (list == nullptr) {
               list = lCreateList("", QU_Type);
               lSetList(cqueue, CQ_qinstances, list);
            }
            qinstance = qinstance_create(cqueue, answer_list,
                                         hostname, &is_ambiguous, monitor, master_hgroup_list, master_cqueue_list);
            if (is_ambiguous) {
               DPRINTF(("qinstance %s has ambiguous conf\n", hostname));
            }
            lSetUlong(qinstance, QU_tag, SGE_QI_TAG_ADD);
            lAppendElem(list, qinstance);
         }
      }
   }
   DRETURN(ret);
}

static bool
cqueue_mark_qinstances(lListElem *cqueue, lList **answer_list, lList *del_hosts) {
   bool ret = true;

   DENTER(TOP_LAYER);
   if (cqueue != nullptr) {
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
      lListElem *qinstance;

      for_each_rw(qinstance, qinstance_list) {
         const char *hostname = lGetHost(qinstance, QU_qhostname);
         const lListElem *href = lGetElemHost(del_hosts, HR_name, hostname);

         if (href != nullptr) {
            if (qinstance_slots_used(qinstance) > 0 || qinstance_slots_reserved(qinstance) > 0) {
               /*
                * Jobs are currently running in this queue. Therefore
                * it is not possible to delete the queue but we
                * will set it into the "orphaned" state 
                */
               sge_qmaster_qinstance_state_set_orphaned(qinstance, true);
               lSetUlong(qinstance, QU_tag, SGE_QI_TAG_MOD);
            } else {
               lSetUlong(qinstance, QU_tag, SGE_QI_TAG_DEL);
            }
         } else {
            lSetUlong(qinstance, QU_tag, SGE_QI_TAG_DEFAULT);
         }
      }
   }
   DRETURN(ret);
}

static bool
cqueue_mod_attributes(lListElem *cqueue, lList **answer_list,
                      lListElem *reduced_elem, int sub_command) {
   bool ret = true;

   DENTER(TOP_LAYER);
   if (cqueue != nullptr && reduced_elem != nullptr) {
      const char *cqueue_name = lGetString(cqueue, CQ_name);
      int index = 0;

      while (cqueue_attribute_array[index].cqueue_attr != NoName && ret) {
         int pos = lGetPosViaElem(reduced_elem,
                                  cqueue_attribute_array[index].cqueue_attr, SGE_NO_ABORT);

         if (pos >= 0) {
            ret &= cqueue_mod_sublist(cqueue, answer_list, reduced_elem,
                                      sub_command,
                                      cqueue_attribute_array[index].cqueue_attr,
                                      cqueue_attribute_array[index].href_attr,
                                      cqueue_attribute_array[index].value_attr,
                                      cqueue_attribute_array[index].primary_key_attr,
                                      cqueue_attribute_array[index].name,
                                      cqueue_name);
         }
         index++;
      }
   }
   DRETURN(ret);
}

static bool
cqueue_mod_hostlist(lListElem *cqueue, lList **answer_list, lListElem *reduced_elem, int sub_command, lList **add_hosts,
                    lList **rem_hosts) {
   bool ret = true;
   const lList *master_hgroup_list = *object_type_get_master_list(SGE_TYPE_HGROUP);

   DENTER(TOP_LAYER);
   if (cqueue != nullptr && reduced_elem != nullptr) {
      int pos = lGetPosViaElem(reduced_elem, CQ_hostlist, SGE_NO_ABORT);

      if (pos >= 0) {
         const char *cqueue_name = lGetString(cqueue, CQ_name);
         lList *list = lGetPosList(reduced_elem, pos);
         lList *old_href_list = lCopyList("", lGetList(cqueue, CQ_hostlist));
         const lList *href_list = nullptr;
         lList *add_groups = nullptr;
         lList *rem_groups = nullptr;

         ret &= href_list_resolve_hostnames(list, answer_list, true);
         if (ret) {
            ret = attr_mod_sub_list(answer_list, cqueue, CQ_hostlist, HR_name,
                                    reduced_elem, sub_command,
                                    SGE_ATTR_HOST_LIST,
                                    cqueue_name, 0, nullptr);
            href_list = lGetList(cqueue, CQ_hostlist);
         }
         if (ret) {
            ret &= href_list_find_diff(href_list, answer_list, old_href_list,
                                       add_hosts, rem_hosts, &add_groups,
                                       &rem_groups);
         }
         if (ret && add_groups != nullptr) {
            ret &= hgroup_list_exists(master_hgroup_list, answer_list, add_groups);
         }
         if (ret) {
            ret &= href_list_find_effective_diff(answer_list, add_groups,
                                                 rem_groups, master_hgroup_list,
                                                 add_hosts, rem_hosts);
         }
         if (ret) {
            ret &= href_list_resolve_hostnames(*add_hosts, answer_list, false);
         }

         /*
          * Make sure that:
          *   - added hosts where not already part the old hostlist
          *   - removed hosts are not part of the new hostlist
          */
         if (ret) {
            lList *tmp_hosts = nullptr;

            ret &= href_list_find_all_references(old_href_list, answer_list,
                                                 master_hgroup_list, &tmp_hosts, nullptr);
            ret &= href_list_remove_existing(add_hosts, answer_list, tmp_hosts);
            lFreeList(&tmp_hosts);

            ret &= href_list_find_all_references(href_list, answer_list,
                                                 master_hgroup_list, &tmp_hosts, nullptr);
            ret &= href_list_remove_existing(rem_hosts, answer_list, tmp_hosts);
            lFreeList(&tmp_hosts);
         }

#if 0 /* EB: DEBUG */
         if (ret) {
            href_list_debug_print(*add_hosts, "add_hosts: ");
            href_list_debug_print(*rem_hosts, "rem_hosts: ");
         }
#endif

         lFreeList(&old_href_list);
         lFreeList(&add_groups);
         lFreeList(&rem_groups);
      }
   }
   DRETURN(ret);
}

bool
cqueue_mod_qinstances(lListElem *cqueue, lList **answer_list, lListElem *reduced_elem,
                      bool refresh_all_values, bool is_startup, monitoring_t *monitor, const lList *master_hgroup_list,
                      lList *master_cqueue_list) {
   dstring buffer = DSTRING_INIT;
   bool ret = true;
   const lList *master_userset_list = *object_type_get_master_list(SGE_TYPE_USERSET);
   const lList *master_ar_list = *object_type_get_master_list(SGE_TYPE_AR);

   DENTER(TOP_LAYER);

   if (cqueue != nullptr && reduced_elem != nullptr) {
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
      lListElem *qinstance;

      /*
       * Try to find changes for all qinstances ...
       */
      for_each_rw(qinstance, qinstance_list) {
         const char *qinstance_name = qinstance_get_name(qinstance, &buffer);
         bool is_ambiguous = qinstance_state_is_ambiguous(qinstance);
         bool is_del = (lGetUlong(qinstance, QU_tag) == SGE_QI_TAG_DEL) ? true : false;
         bool will_be_ambiguous = false;
         bool state_changed = false;
         bool conf_changed = false;
         int index = 0;
         bool need_reinitialize = false;

         /*
          * Set full name of QI if it is not set
          */
         if (refresh_all_values &&
             lGetString(qinstance, QU_full_name) == nullptr) {
            qinstance_set_full_name(qinstance);
         }

         /* 
          * Clear all messages which explain ambiguous state
          */
         qinstance_message_trash_all_of_type_X(qinstance, QI_AMBIGUOUS);

         /*
          * Handle each cqueue attribute as long as there was no error
          * and only if the qinstance won't be deleted afterward.
          */

         while (ret && !is_del &&
                cqueue_attribute_array[index].cqueue_attr != NoName) {
            const char *matching_host_or_group = nullptr;
            const char *matching_group = nullptr;

            int pos = lGetPosViaElem(reduced_elem,
                                     cqueue_attribute_array[index].cqueue_attr, SGE_NO_ABORT);


            /*
             * We try to find changes only for attributes which were 
             * sent by the client. Only for those attributes 'pos' will
             * be >= 0.
             *
             * There are two situations which make it absolutely necessary
             * to have a look on ALL attributes:
             *
             * 1) refresh_all_values == true
             *    The hostlist of "cqueue" changed. As a result it
             *    might be possible that a value for an attribute is
             *    now ambiguous. 
             * 
             * 2) is_ambiguous == true
             *    The qinstance is currently in the ambiguous state.
             *    It is not enough to test only modified attributes if
             *    they are nonambigous. It is also necesssary to check
             *    if all attributes which are not changed now are
             *    nonambigous to clear the ambigous-state from qinstance. 
             */
            if (pos >= 0 || refresh_all_values || is_ambiguous) {
               bool tmp_is_ambiguous = false;
               bool tmp_has_changed_conf_attr = false;
               bool tmp_has_changed_state_attr = false;

               ret &= qinstance_modify_attribute(qinstance,
                                                 answer_list, cqueue,
                                                 cqueue_attribute_array[index].qinstance_attr,
                                                 cqueue_attribute_array[index].cqueue_attr,
                                                 cqueue_attribute_array[index].href_attr,
                                                 cqueue_attribute_array[index].value_attr,
                                                 cqueue_attribute_array[index].primary_key_attr,
                                                 &matching_host_or_group,
                                                 &matching_group,
                                                 &tmp_is_ambiguous,
                                                 &tmp_has_changed_conf_attr,
                                                 &tmp_has_changed_state_attr,
                                                 is_startup,
                                                 &need_reinitialize,
                                                 monitor, master_hgroup_list, master_cqueue_list);

               if (tmp_is_ambiguous) {
                  /*
                   * Add a message which explains the reason for
                   * ambiguous state
                   */
                  sprintf(SGE_EVENT, MSG_ATTR_HASAMBVAL_SSS,
                          cqueue_attribute_array[index].name,
                          matching_host_or_group, matching_group);
                  qinstance_message_add(qinstance, QI_AMBIGUOUS, SGE_EVENT);
               }

               will_be_ambiguous |= tmp_is_ambiguous;
               state_changed |= tmp_has_changed_state_attr;
               conf_changed |= tmp_has_changed_conf_attr;
            }

            index++;
         }

         if (need_reinitialize) {
            qinstance_reinit_consumable_actual_list(qinstance, answer_list);
         }

         /*
          * Change qinstance state
          */
         sge_qmaster_qinstance_state_set_ambiguous(qinstance, will_be_ambiguous);
         if (will_be_ambiguous && !is_ambiguous) {
            state_changed = true;
            DPRINTF(("Qinstance "SFQ" has ambiguous configuration\n",
                    qinstance_name));
         } else if (!will_be_ambiguous && is_ambiguous) {
            state_changed = true;
            DPRINTF(("Qinstance "SFQ" has non-ambiguous configuration\n",
                    qinstance_name));
         }

         /*
          * Tag the qinstance as modified if the internal state changed. 
          * This will result in spooling the qinstance. Also mod-events wiil 
          * be sent. If only the configuration changed than it is only 
          * necessary to send mod-events.
          */
         if (state_changed) {
            DPRINTF(("Internal state of qinstance "SFQ" has been changed\n",
                    qinstance_name));
            lSetUlong(qinstance, QU_tag, SGE_QI_TAG_MOD);
            qinstance_increase_qversion(qinstance);
         } else if (conf_changed) {
            DPRINTF(("Only config value of qinstance "SFQ" has been changed\n",
                    qinstance_name));
            lSetUlong(qinstance, QU_tag, SGE_QI_TAG_MOD_ONLY_CONFIG);
            qinstance_increase_qversion(qinstance);
         }

         if (ret && !is_startup) {
            lListElem *ar;

            for_each_rw(ar, master_ar_list) {
               if (lGetElemStr(lGetList(ar, AR_granted_slots), JG_qname, qinstance_name)) {
                  if (!sge_ar_have_users_access(nullptr, ar, lGetString(qinstance, QU_full_name),
                                                lGetList(qinstance, QU_acl), lGetList(qinstance, QU_xacl),
                                                master_userset_list)) {
                     ERROR((SGE_EVENT, MSG_PARSE_MOD3_REJECTED_DUE_TO_AR_SU, SGE_ATTR_USER_LISTS, sge_u32c(
                             lGetUlong(ar, AR_id))));
                     answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                     ret = false;
                     break;
                  }
               }
            }
         }

         if (!ret) {
            /*
             * Skip remaining qinstances if an error occured.
             */
            break;
         }
      }
   }
   sge_dstring_free(&buffer);

   DRETURN(ret);
}

bool
cqueue_handle_qinstances(lListElem *cqueue, lList **answer_list, lListElem *reduced_elem,
                         lList *add_hosts, lList *rem_hosts, bool refresh_all_values, monitoring_t *monitor,
                         const lList *master_hgroup_list, lList *master_cqueue_list) {
   bool ret = true;

   DENTER(TOP_LAYER);

   if (ret) {
      ret = cqueue_mark_qinstances(cqueue, answer_list, rem_hosts);
   }
   if (ret) {
      ret = cqueue_mod_qinstances(cqueue, answer_list, reduced_elem,
                                  refresh_all_values, false, monitor, master_hgroup_list, master_cqueue_list);
   }
   if (ret) {
      ret = cqueue_add_qinstances(cqueue, answer_list, add_hosts, monitor, master_hgroup_list, master_cqueue_list);
   }
   DRETURN(ret);
}

int
cqueue_mod(lList **answer_list, lListElem *cqueue, lListElem *reduced_elem, int add,
           const char *remote_user, const char *remote_host, gdi_object_t *object, int sub_command,
           monitoring_t *monitor) {
   bool ret = true;
   lList *add_hosts = nullptr;
   lList *rem_hosts = nullptr;
   const lList *master_calendar_list = *object_type_get_master_list(SGE_TYPE_CALENDAR);
   const lList *master_ckpt_list = *object_type_get_master_list(SGE_TYPE_CKPT);
   const lList *master_pe_list = *object_type_get_master_list(SGE_TYPE_PE);
   const lList *master_userset_list = *object_type_get_master_list(SGE_TYPE_USERSET);
   const lList *master_project_list = *object_type_get_master_list(SGE_TYPE_PROJECT);
   const lList *master_centry_list = *object_type_get_master_list(SGE_TYPE_CENTRY);
   const lList *master_hgroup_list = *object_type_get_master_list(SGE_TYPE_HGROUP);
   const lList *master_ehost_list = *object_type_get_master_list(SGE_TYPE_EXECHOST);
   lList *master_cqueue_list = *object_type_get_master_list_rw(SGE_TYPE_CQUEUE);

   DENTER(TOP_LAYER);

   int pos = lGetPosViaElem(reduced_elem, CQ_name, SGE_NO_ABORT);
   if (pos >= 0) {
      const char *name = lGetPosString(reduced_elem, pos);

      if (add) {
         if (verify_str_key(
                 answer_list, name, MAX_VERIFY_STRING, "cqueue", KEY_TABLE) == STATUS_OK) {
            DTRACE;
            lSetString(cqueue, CQ_name, name);
         } else {
            ERROR((SGE_EVENT, MSG_CQUEUE_NAMENOTGUILTY_S, name));
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX,
                            ANSWER_QUALITY_ERROR);
            ret = false;
         }
      } else {
         const char *old_name = lGetString(cqueue, CQ_name);

         if (strcmp(old_name, name)) {
            ERROR((SGE_EVENT, SFNMAX, MSG_CQUEUE_NONAMECHANGE));
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX,
                            ANSWER_QUALITY_ERROR);
            ret = false;
         }
      }
   } else {
      ERROR((SGE_EVENT, MSG_SGETEXT_MISSINGCULLFIELD_SS,
              lNm2Str(CQ_name), __func__));
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN,
                      ANSWER_QUALITY_ERROR);
      ret = false;
   }

   /*
    * Find differences of hostlist configuration
    */
   if (ret) {
      ret &= cqueue_mod_hostlist(cqueue, answer_list, reduced_elem,
                                 sub_command, &add_hosts, &rem_hosts);
   }

   /*
    * Its time to do the cqueue modifications:
    *    - change the attribute lists in the cqueue object
    *    - verify the attribute lists
    */
   if (ret) {
      ret &= cqueue_mod_attributes(cqueue, answer_list,
                                   reduced_elem, sub_command);
   }
   if (ret) {
      ret &= cqueue_verify_attributes(cqueue, answer_list,
                                      reduced_elem, true, master_calendar_list, master_ckpt_list,
                                      master_pe_list, master_userset_list, master_project_list,
                                      master_centry_list, master_cqueue_list, master_hgroup_list);
   }

   /*
    * Now we have to add/mod/del all qinstances
    */
   if (ret) {
      bool refresh_all_values = ((add_hosts != nullptr) || (rem_hosts != nullptr)) ? true : false;

      ret &= cqueue_handle_qinstances(cqueue, answer_list, reduced_elem,
                                      add_hosts, rem_hosts, refresh_all_values, monitor, master_hgroup_list,
                                      master_cqueue_list);
   }

   /*
    * Client and scheduler code expects existing EH_Type elements
    * for all hosts used in CQ_hostlist. Therefore it is neccessary
    * to create all not existing EH_Type elements.
    */
   if (ret) {
      ret &= host_list_add_missing_href(master_ehost_list, answer_list, add_hosts, monitor);
   }

   /*
    * Cleanup
    */
   lFreeList(&add_hosts);
   lFreeList(&rem_hosts);

   if (ret) {
      DRETURN(0);
   } else {
      DRETURN(STATUS_EUNKNOWN);
   }
}

int
cqueue_success(lListElem *cqueue, lListElem *old_cqueue, gdi_object_t *object, lList **ppList, monitoring_t *monitor) {
   const lList *qinstances;
   lListElem *qinstance;
   DENTER(TOP_LAYER);
   const lList *master_job_list = *object_type_get_master_list(SGE_TYPE_JOB);

   cqueue_update_categories(cqueue, old_cqueue);

   /*
    * CQ modify or add event
    */
   sge_add_event(0, old_cqueue ? sgeE_CQUEUE_MOD : sgeE_CQUEUE_ADD, 0, 0,
                 lGetString(cqueue, CQ_name), nullptr, nullptr, cqueue);

   /*
    * QI modify, add or delete event. Finalize operation.
    */
   cqueue_commit(cqueue);

   /*
    * Handle jobs which were supended due to suspend threshold
    */
   qinstances = lGetList(cqueue, CQ_qinstances);

   for_each_rw(qinstance, qinstances) {
      /* check slotwise subordinate suspends for new qinstance config */
      do_slotwise_x_on_subordinate_check(qinstance, false, false, monitor);
      do_slotwise_x_on_subordinate_check(qinstance, true, false, monitor);

      if (lGetUlong(qinstance, QU_gdi_do_later) == GDI_DO_LATER) {
         bool is_qinstance_mod = false;
         const char *full_name = lGetString(qinstance, QU_full_name);
         lListElem *job;

         lSetUlong(qinstance, QU_gdi_do_later, 0);

         /* in case the thresholds are set to none, we have to unsuspend all jobs because
            the scheduler is not able to do that. If the suspend threshold is still set; 
            just changed, the scheduler can easily deal with it.*/
         if (lGetList(qinstance, QU_suspend_thresholds) == nullptr) {
            for_each_rw(job, master_job_list) {
               const lList *ja_tasks = lGetList(job, JB_ja_tasks);
               lListElem *ja_task;

               for_each_rw(ja_task, ja_tasks) {
                  u_long32 state = lGetUlong(ja_task, JAT_state);

                  if (ISSET(state, JSUSPENDED_ON_THRESHOLD)) {
                     /* this does most likely not work with pe jobs, which run in different queues.
                        Issue: 831*/
                     const char *queue_name = lGetString(lFirst(lGetList(ja_task,
                                                                         JAT_granted_destin_identifier_list)),
                                                         JG_qname);

                     if (!strcmp(queue_name, full_name)) {

                        if (!ISSET(state, JSUSPENDED)) {
                           sge_signal_queue(SGE_SIGCONT, qinstance, job, ja_task, monitor);
                           SETBIT(JRUNNING, state);
                           is_qinstance_mod = true;
                        }

                        CLEARBIT(JSUSPENDED_ON_THRESHOLD, state);

                        lSetUlong(ja_task, JAT_state, state);

                        sge_event_spool(nullptr, 0, sgeE_JATASK_MOD,
                                        lGetUlong(job, JB_job_number),
                                        lGetUlong(ja_task, JAT_task_number), nullptr, nullptr, nullptr,
                                        job, ja_task, nullptr, true, true);

                     }
                  }
               }
            }
         }

         if (is_qinstance_mod) {
            const char *hostname = lGetHost(qinstance, QU_qhostname);
            const char *cqueue_name = lGetString(qinstance, QU_qname);
            sge_event_spool(nullptr, 0, sgeE_QINSTANCE_MOD,
                            0, 0, cqueue_name, hostname, nullptr,
                            qinstance, nullptr, nullptr, true, true);
         }
      }
   }

   DRETURN(0);
}

void
cqueue_commit(lListElem *cqueue) {
   lList *qinstances = lGetListRW(cqueue, CQ_qinstances);
   lListElem *qinstance = nullptr;

   DENTER(TOP_LAYER);

   /*
    * QI modify, add or delete event
    */
   lListElem *next_qinstance = lFirstRW(qinstances);
   while ((qinstance = next_qinstance)) {
      u_long32 tag = lGetUlong(qinstance, QU_tag);
      const char *name = lGetString(qinstance, QU_qname);
      const char *hostname = lGetHost(qinstance, QU_qhostname);

      next_qinstance = lNextRW(qinstance);

      /*
       * Reset QI tag
       */
      lSetUlong(qinstance, QU_tag, SGE_QI_TAG_DEFAULT);

      if (tag == SGE_QI_TAG_ADD) {
         sge_add_event(0, sgeE_QINSTANCE_ADD, 0, 0,
                       name, hostname, nullptr, qinstance);
      } else if (tag == SGE_QI_TAG_MOD ||
                 tag == SGE_QI_TAG_MOD_ONLY_CONFIG) {
         sge_add_event(0, sgeE_QINSTANCE_MOD, 0, 0,
                       name, hostname, nullptr, qinstance);
      } else if (tag == SGE_QI_TAG_DEL) {
         sge_event_spool(nullptr, 0, sgeE_QINSTANCE_DEL, 0, 0, name, hostname,
                         nullptr, nullptr, nullptr, nullptr, true, true);

         /*
          * Now we can remove the qinstance.
          */
         lRemoveElem(qinstances, &qinstance);
      }
   }
   if (lGetNumberOfElem(qinstances) == 0) {
      lSetList(cqueue, CQ_qinstances, nullptr);
   }
   DRETURN_VOID;
}

int
cqueue_spool(lList **answer_list, lListElem *cqueue, gdi_object_t *object) {
   int ret = 0;
   const char *name = lGetString(cqueue, CQ_name);
   lListElem *qinstance;
   dstring key_dstring = DSTRING_INIT;
   bool dbret;
   lList *spool_answer_list = nullptr;

   DENTER(TOP_LAYER);
   dbret = spool_write_object(&spool_answer_list, spool_get_default_context(), cqueue, name, SGE_TYPE_CQUEUE, true);
   answer_list_output(&spool_answer_list);

   if (!dbret) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_PERSISTENCE_WRITE_FAILED_S, name);
      ret = 1;
   }

   for_each_rw(qinstance, lGetList(cqueue, CQ_qinstances)) {
      u_long32 tag = lGetUlong(qinstance, QU_tag);

      if (tag == SGE_QI_TAG_ADD || tag == SGE_QI_TAG_MOD) {
         const char *key =
                 sge_dstring_sprintf(&key_dstring, "%s/%s", name, lGetHost(qinstance, QU_qhostname));
         dbret = spool_write_object(&spool_answer_list, spool_get_default_context(), qinstance,
                                    key, SGE_TYPE_QINSTANCE, true);
         answer_list_output(&spool_answer_list);

         if (!dbret) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_PERSISTENCE_WRITE_FAILED_S, key);
            ret = 1;
         }
      }
   }

   sge_dstring_free(&key_dstring);

   DRETURN(ret);
}

int
cqueue_del(lListElem *this_elem, lList **answer_list, char *remote_user, char *remote_host) {
   bool ret = true;
   lList *master_cqueue_list = *object_type_get_master_list_rw(SGE_TYPE_CQUEUE);

   DENTER(TOP_LAYER);

   if (this_elem != nullptr && remote_user != nullptr && remote_host != nullptr) {
      const char *name = lGetString(this_elem, CQ_name);

      if (name != nullptr) {
         lListElem *cqueue = cqueue_list_locate(master_cqueue_list, name);

         if (cqueue != nullptr) {
            const lList *qinstances = lGetList(cqueue, CQ_qinstances);
            lListElem *qinstance = nullptr;
            const char *cq_name = lGetString(cqueue, CQ_name);
            dstring dir = DSTRING_INIT;
            bool do_del = true;

/* TODO: HP: Trigger recalculation of ssos for all subqueues of this queue */
            /*
             * test if the CQ can be removed
             */
            for_each_rw(qinstance, qinstances) {
               if (qinstance_slots_used(qinstance) > 0 || qinstance_slots_reserved(qinstance) > 0) {
                  ERROR((SGE_EVENT, SFNMAX, MSG_QINSTANCE_STILLJOBS));
                  answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST,
                                  ANSWER_QUALITY_ERROR);
                  do_del = false;
                  break;
               }
            }

            /*
             * check for references of this cqueue in other cqueues subordinate
             * lists
             */
            if (do_del) {
               const lListElem *tmp_cqueue;

               for_each_ep(tmp_cqueue, master_cqueue_list) {

                  if (cqueue_is_used_in_subordinate(name, tmp_cqueue)) {
                     ERROR((SGE_EVENT, MSG_CQUEUE_DEL_ISREFASSUBORDINATE_SS,
                             name, lGetString(tmp_cqueue, CQ_name)));
                     answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN,
                                     ANSWER_QUALITY_ERROR);
                     do_del = false;
                     break;
                  }
               }
            }

            if (do_del) {
               /*
                * delete QIs
                */
               dstring key = DSTRING_INIT;
               sge_dstring_sprintf(&dir, "%s/%s", QINSTANCES_DIR, cq_name);

               for_each_rw(qinstance, qinstances) {
                  const char *qi_name = lGetHost(qinstance, QU_qhostname);

                  sge_dstring_sprintf(&key, "%s/%s", cq_name, qi_name);
                  if (sge_event_spool(answer_list, 0, sgeE_QINSTANCE_DEL,
                                      0, 0, cq_name, qi_name,
                                      nullptr, nullptr, nullptr, nullptr, true, true)) { ;
                  }
               }
               sge_dstring_free(&key);
               sge_rmdir(sge_dstring_get_string(&dir), nullptr);
               sge_dstring_free(&dir);

               /*
                * delete CQ
                */
               if (sge_event_spool(answer_list, 0, sgeE_CQUEUE_DEL, 0, 0, name, nullptr, nullptr,
                                   nullptr, nullptr, nullptr, true, true)) {
                  cqueue_update_categories(nullptr, cqueue);
                  lRemoveElem(master_cqueue_list, &cqueue);

                  INFO((SGE_EVENT, MSG_SGETEXT_REMOVEDFROMLIST_SSSS, remote_user, remote_host, name, "cluster queue"));
                  answer_list_add(answer_list, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
               } else {
                  ERROR((SGE_EVENT, MSG_CANTSPOOL_SS, "cluster queue", name));
                  answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
                  ret = false;
               }
            } else {
               ret = false;
            }
         } else {
            ERROR((SGE_EVENT, MSG_SGETEXT_DOESNOTEXIST_SS, "cluster queue", name));
            answer_list_add(answer_list, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
            ret = false;
         }
      } else {
         ERROR((SGE_EVENT, MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(CQ_name), __func__));
         answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = false;
      }
   } else {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, __func__));
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   if (ret) {
      DRETURN(STATUS_OK);
   } else {
      DRETURN(STATUS_EUNKNOWN);
   }
}

bool
cqueue_del_all_orphaned(lListElem *this_elem, lList **answer_list, const char *ehname) {
   bool ret = true;

   DENTER(TOP_LAYER);

   if (this_elem != nullptr) {
      dstring dir = DSTRING_INIT;
      const char *cq_name = lGetString(this_elem, CQ_name);
      lList *qinstance_list = lGetListRW(this_elem, CQ_qinstances);
      lListElem *qinstance = nullptr;

      if (ehname) {
         if ((qinstance = lGetElemHostRW(qinstance_list, QU_qhostname, ehname)) &&
             qinstance_state_is_orphaned(qinstance) &&
             qinstance_slots_used(qinstance) == 0 &&
             qinstance_slots_reserved(qinstance) == 0) {
            const char *qi_name = lGetHost(qinstance, QU_qhostname);

            /*
             * This qinstance should be deleted. There are not jobs anymore.
             */
            sge_dstring_sprintf(&dir, "%s/%s", QINSTANCES_DIR, cq_name);
            if (sge_event_spool(answer_list, 0, sgeE_QINSTANCE_DEL, 0, 0, cq_name, qi_name,
                                nullptr, nullptr, nullptr, nullptr, true, true)) {
               lRemoveElem(qinstance_list, &qinstance);
               if (lGetNumberOfElem(qinstance_list) == 0) {
                  sge_rmdir(sge_dstring_get_string(&dir), nullptr);
               }
            }
         }
      } else {
         lListElem *next_qinstance = lFirstRW(qinstance_list);
         while ((qinstance = next_qinstance) != nullptr) {
            next_qinstance = lNextRW(qinstance);

            if (qinstance_state_is_orphaned(qinstance) &&
                qinstance_slots_used(qinstance) == 0 &&
                qinstance_slots_reserved(qinstance) == 0) {
               const char *qi_name = lGetHost(qinstance, QU_qhostname);

               /*
                * This qinstance should be deleted. There are not jobs anymore.
                */
               sge_dstring_sprintf(&dir, "%s/%s", QINSTANCES_DIR, cq_name);
               if (sge_event_spool(answer_list, 0, sgeE_QINSTANCE_DEL, 0, 0, cq_name, qi_name,
                                   nullptr, nullptr, nullptr, nullptr, true, true)) {
                  lRemoveElem(qinstance_list, &qinstance);
                  if (lGetNumberOfElem(qinstance_list) == 0) {
                     sge_rmdir(sge_dstring_get_string(&dir), nullptr);
                  }
               }
            }
         }
      }
      sge_dstring_free(&dir);
   }

   DRETURN(ret);
}

bool
cqueue_list_del_all_orphaned(lList *this_list, lList **answer_list, const char *cqname, const char *ehname) {
   bool ret = true;
   lListElem *cqueue;

   DENTER(TOP_LAYER);

   if (cqname) {
      cqueue = lGetElemStrRW(this_list, CQ_name, cqname);
      ret &= cqueue_del_all_orphaned(cqueue, answer_list, ehname);
   } else {
      for_each_rw(cqueue, this_list) {
         ret &= cqueue_del_all_orphaned(cqueue, answer_list, ehname);
         if (!ret) {
            break;
         }
      }
   }

   DRETURN(ret);
}

void
cqueue_list_set_unknown_state(lList *this_list, const char *hostname, bool send_events, bool is_unknown) {
   const lListElem *cqueue;

   for_each_ep(cqueue, this_list) {
      if (hostname != nullptr) {
         const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
         lListElem *qinstance = lGetElemHostRW(qinstance_list, QU_qhostname, hostname);

         if (qinstance != nullptr && qinstance_state_is_unknown(qinstance) != is_unknown) {
            sge_qmaster_qinstance_state_set_unknown(qinstance, is_unknown);
            if (send_events) {
               qinstance_add_event(qinstance, sgeE_QINSTANCE_MOD);
            }
         }
      } else {
         lListElem *qinstance;

         for_each_rw (qinstance, lGetList(cqueue, CQ_qinstances)) {
            if (qinstance_state_is_unknown(qinstance) != is_unknown) {
               sge_qmaster_qinstance_state_set_unknown(qinstance, is_unknown);
               if (send_events) {
                  qinstance_add_event(qinstance, sgeE_QINSTANCE_MOD);
               }
            }
         }
      }
   }
}


/****** sge_cqueue_qmaster/cqueue_diff_sublist() *******************************
*  NAME
*     cqueue_diff_sublist() -- Diff cluster queue sublists
*
*  SYNOPSIS
*     static void cqueue_diff_sublist(const lListElem *new_cqueue, const lListElem
*     *old_cqueue, int snm1, int snm2, int sublist_nm, int key_nm, const lDescr *dp,
*     lList **new_sublist, lList **old_sublist)
*
*  FUNCTION
*     Determine new/old refereneces in a cluster queue configuration sublist.
*
*  INPUTS
*     const lListElem *new_cqueue - New cluster queue (CQ_Type)
*     const lListElem *old_cqueue - Old cluster queue (CQ_Type)
*     int snm1             - First cluster queue sublist field
*     int snm2             - Second cluster queue sublist field
*     int sublist_nm       - Subsub list field
*     int key_nm           - Field with key in subsublist
*     const lDescr *dp     - Type for outgoing sublist arguments
*     lList **new_sublist  - List of new references
*     lList **old_sublist  - List of old references
*
*  NOTES
*     MT-NOTE: cqueue_diff_sublist() is MT safe
*******************************************************************************/
static void
cqueue_diff_sublist(const lListElem *new_cqueue, const lListElem *old_cqueue, int snm1, int snm2, int sublist_nm,
                    int key_nm, const lDescr *dp, lList **new_sublist, lList **old_sublist) {
   const lListElem *qc, *ep;
   const char *p;

   DENTER(TOP_LAYER);

   /* collect 'old' entries in 'old_sublist' */
   if (old_cqueue && old_sublist) {
      for_each_ep(qc, lGetList(old_cqueue, snm1)) {
         for_each_ep(ep, lGetList(qc, sublist_nm)) {
            p = lGetString(ep, key_nm);
            if (!lGetElemStr(*old_sublist, key_nm, p))
               lAddElemStr(old_sublist, key_nm, p, dp);
         }
      }
      for_each_ep(qc, lGetList(old_cqueue, snm2)) {
         for_each_ep(ep, lGetList(qc, sublist_nm)) {
            p = lGetString(ep, key_nm);
            if (!lGetElemStr(*old_sublist, key_nm, p))
               lAddElemStr(old_sublist, key_nm, p, dp);
         }
      }
   }

   /* collect 'new' entries in 'new_sublist' */
   if (new_cqueue && new_sublist) {
      for_each_ep(qc, lGetList(new_cqueue, snm1)) {
         for_each_ep(ep, lGetList(qc, sublist_nm)) {
            p = lGetString(ep, key_nm);
            if (!lGetElemStr(*new_sublist, key_nm, p))
               lAddElemStr(new_sublist, key_nm, p, dp);
         }
      }
      for_each_ep(qc, lGetList(new_cqueue, snm2)) {
         for_each_ep(ep, lGetList(qc, sublist_nm)) {
            p = lGetString(ep, key_nm);
            if (!lGetElemStr(*new_sublist, key_nm, p))
               lAddElemStr(new_sublist, key_nm, p, dp);
         }
      }
   }

   DRETURN_VOID;
}

/****** sge_cqueue_qmaster/cqueue_diff_projects() ******************************
*  NAME
*     cqueue_diff_projects() -- Diff old/new cluster queue projects
*
*  SYNOPSIS
*     void cqueue_diff_projects(const lListElem *new_cqueue, const lListElem *old_cqueue,
*     lList **new_prj, lList **old_prj)
*
*  FUNCTION
*     A diff new/old is made regarding cluster queue projects/xprojects.
*     Project references are returned in new_prj/old_prj.
*
*  INPUTS
*     const lListElem *new_cqueue - New cluster queue (CQ_Type)
*     const lListElem *old_cqueue - Old cluster queue (CQ_Type)
*     lList **new_prj      - New project references (PR_Type)
*     lList **old_prj      - Old project references (PR_Type)
*
*  NOTES
*     MT-NOTE: cqueue_diff_projects() is MT safe
*******************************************************************************/
void
cqueue_diff_projects(const lListElem *new_cqueue, const lListElem *old_cqueue, lList **new_prj, lList **old_prj) {
   cqueue_diff_sublist(new_cqueue, old_cqueue, CQ_projects, CQ_xprojects,
                       APRJLIST_value, PR_name, PR_Type, new_prj, old_prj);
   lDiffListStr(PR_name, new_prj, old_prj);
}

/****** sge_cqueue_qmaster/cqueue_diff_usersets() ******************************
*  NAME
*     cqueue_diff_projects() -- Diff old/new cluster queue usersets
*
*  SYNOPSIS
*     void cqueue_diff_projects(const lListElem *new_cqueue, const lListElem *old_cqueue,
*     lList **new_prj, lList **old_prj)
*
*  FUNCTION
*     A diff new/old is made regarding cluster queue acl/xacl.
*     Userset references are returned in new_acl/old_acl.
*
*  INPUTS
*     const lListElem *new_cqueue - New cluster queue (CQ_Type)
*     const lListElem *old_cqueue - Old cluster queue (CQ_Type)
*     lList **new_acl      - New userset references (US_Type)
*     lList **old_acl      - Old userset references (US_Type)
*
*  NOTES
*     MT-NOTE: cqueue_diff_usersets() is MT safe
*******************************************************************************/
void
cqueue_diff_usersets(const lListElem *new_cqueue, const lListElem *old_cqueue, lList **new_acl, lList **old_acl) {
   cqueue_diff_sublist(new_cqueue, old_cqueue, CQ_acl, CQ_xacl,
                       AUSRLIST_value, US_name, US_Type, new_acl, old_acl);
   lDiffListStr(US_name, new_acl, old_acl);
}


/****** sge_cqueue_qmaster/cqueue_update_categories() **************************
*  NAME
*     cqueue_update_categories() -- Update categories wrts userset/project
*
*  SYNOPSIS
*     static void cqueue_update_categories(const lListElem *new_cq, const
*     lListElem *old_cq)
*
*  FUNCTION
*     The userset/project information wrts categories is updated based
*     on new/old cluster queue configuration and events are sent upon
*     changes.
*
*  INPUTS
*     const lListElem *new_cq - New cluster queue (CQ_Type)
*     const lListElem *old_cq - Old cluster queue (CQ_Type)
*
*  NOTES
*     MT-NOTE: cqueue_update_categories() is not MT safe
*******************************************************************************/
static void
cqueue_update_categories(const lListElem *new_cq, const lListElem *old_cq) {
   lList *old_lp = nullptr, *new_lp = nullptr;

   cqueue_diff_projects(new_cq, old_cq, &new_lp, &old_lp);
   project_update_categories(new_lp, old_lp);
   lFreeList(&old_lp);
   lFreeList(&new_lp);

   cqueue_diff_usersets(new_cq, old_cq, &new_lp, &old_lp);
   userset_update_categories(new_lp, old_lp);
   lFreeList(&old_lp);
   lFreeList(&new_lp);
}

/****** sgeobj/qinstance/qinstance_check_unknown_state() **********************
*  NAME
*     qinstance_check_unknown_state() -- Modifies the number of used slots 
*
*  SYNOPSIS
*     void
*     qinstance_check_unknown_state(lListElem *this_elem)
*
*  FUNCTION
*     Checks if there are nonstatic load values available for the
*     qinstance. If this is the case, then then the "unknown" state 
*     of that machine will be released. 
*
*  INPUTS
*     lListElem *this_elem - QU_Type 
*
*  RESULT
*     void - NONE 
*
*  NOTES
*     MT-NOTE: qinstance_check_unknown_state() is MT safe 
*******************************************************************************/
static void
qinstance_check_unknown_state(lListElem *this_elem, const lList *master_exechost_list) {
   DENTER(TOP_LAYER);
   const char *hostname = lGetHost(this_elem, QU_qhostname);
   lListElem *host = host_list_locate(master_exechost_list, hostname);

   if (host != nullptr) {
      u_long32 last_heard = lGetUlong(host, EH_lt_heard_from);

      if (last_heard != 0) {
         sge_qmaster_qinstance_state_set_unknown(this_elem, false);
      }
   }
   DRETURN_VOID;
}
