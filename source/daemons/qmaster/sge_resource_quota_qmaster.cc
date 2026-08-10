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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Resource quota sets
 */
#include <cstring>
#include <fnmatch.h>

#include "uti/ocs_Pattern.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge.h"
#include "uti/sge_hostname.h"

#include "sgeobj/sge_host.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/ocs_DataStore.h"

#include "spool/sge_spooling.h"

#include "evm/sge_event_master.h"

#include "sge_userprj_qmaster.h"
#include "sge_userset_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "sge_utility_qmaster.h"
#include "sge_resource_quota_qmaster.h"
#include "msg_common.h"
#include "msg_qmaster.h"

static bool
rqs_reinit_consumable_actual_list(lListElem *rqs, lList **answer_list);

static void
rqs_update_categories(const lListElem *new_rqs, const lListElem *old_rqs, uint64_t gdi_session);

static bool
filter_diff_usersets_or_projects(const lListElem *rule, int filter_nm, lList **scope_l, int nm, const lDescr *dp,
                                 const lList *master_list);

static bool
filter_diff_usersets_or_projects_scope(lList *filter_scope, int filter_nm, lList **scope_ref, int nm, const lDescr *dp,
                                       const lList *master_list);

/**
 * @brief Gdi callback function for modifing resource quota sets
 *
 * This function is called from the framework that
 * add/modify/delete generic gdi objects.
 * The purpose of this function is it to add new rqs
 * objects or modify existing resource quota sets.
 *
 * @param alpp reference to an answer list
 * @param new_rqs if a new rqs object will be created by this function, then new_rqs is a newly initialized CULL object. if this function was called due to a modify request than new_rqs will contain the old data
 * @param rqs a reduced rqs object which contains all necessary information to create a new object or modify parts of an existing one
 * @param add 1 if a new element should be added to the master list 0 to modify an existing object
 * @param ruser username who invoked this gdi request
 * @param rhost hostname of where the gdi request was invoked
 * @param object structure of the gdi framework which contains additional information to perform the request
 * @param sub_command how should we handle sublist elements: `SGE_GDI_CHANGE`
 *        modifies them, `SGE_GDI_APPEND` adds to the sublist, `SGE_GDI_REMOVE`
 *        removes from it, `SGE_GDI_SET` replaces the whole sublist
 * @param monitor monitoring structure
 * @param packet the client request
 * @param task the GDI task being answered
 * @param cmd the command being executed
 *
 * @return 0 on success STATUS_EUNKNOWN if an error occurred
 *
 * @note MT-NOTE: rqs_mod() is MT safe
 */
int
rqs_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *new_rqs, lListElem *rqs, int add, const char *ruser,
        const char *rhost, gdi_object_t *object, ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const char *rqs_name = nullptr;
   bool rules_changed = false;
   bool previous_enabled = (bool) lGetBool(new_rqs, RQS_enabled);
   const lList *master_centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);

   /* ---- RQS_name */
   if (add) {
      if (attr_mod_str(alpp, rqs, new_rqs, RQS_name, object->object_name))
         goto ERROR;
   }
   rqs_name = lGetString(new_rqs, RQS_name);

   /* Name has to be a valid name */
   if (add && verify_obj_name(alpp, rqs_name, MAX_VERIFY_STRING,
                              MSG_OBJ_RQS) != STATUS_OK) {
      goto ERROR;
   }

   /* ---- RQS_description */
   attr_mod_zerostr(rqs, new_rqs, RQS_description, "description");

   /* ---- RQS_enabled */
   attr_mod_bool(rqs, new_rqs, RQS_enabled, "enabled");

   /* ---- RQS_rule */
   if (lGetPosViaElem(rqs, RQS_rule, SGE_NO_ABORT) >= 0) {
      rules_changed = true;
      if ((sub_command & ocs::gdi::SubCommand::SET_ALL) == ocs::gdi::SubCommand::SET_ALL) {
         normalize_sublist(rqs, RQS_rule);
         attr_mod_sub_list(alpp, new_rqs, RQS_rule, RQS_name, rqs, cmd, sub_command,
                           SGE_ATTR_RQSRULES, SGE_OBJ_RQS, 0, nullptr);
      } else {
         /* *attr cases */
         const lList *rule_list = lGetList(rqs, RQS_rule);
         const lListElem *rule;

         for_each_ep(rule, rule_list) {
            lList *new_rule_list = lGetListRW(new_rqs, RQS_rule);
            lListElem *new_rule = rqs_rule_locate(new_rule_list, lGetString(rule, RQR_name));
            if (new_rule != nullptr) {
               /* ---- RQR_limit */
               attr_mod_sub_list(alpp, new_rule, RQR_limit, RQRL_name, rule, cmd,
                                 sub_command, SGE_ATTR_RQSRULES, SGE_OBJ_RQS, 0, nullptr);
            } else {
               ERROR(SFNMAX, MSG_RESOURCEQUOTA_NORULEDEFINED);
               answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC,
                               ANSWER_QUALITY_ERROR);
               goto ERROR;
            }
         }
      }
   }

   if (!rqs_verify_attributes(new_rqs, alpp, true, master_centry_list)) {
      goto ERROR;
   }
   if (rules_changed || (lGetBool(new_rqs, RQS_enabled) != previous_enabled)) {
      rqs_reinit_consumable_actual_list(new_rqs, alpp);
   }

   DRETURN(0);

   ERROR:
DRETURN(STATUS_EUNKNOWN);
}

/**
 * @brief Gdi callback funktion to spool a rqs object
 *
 * This function will be called from the framework which will
 * add/modify/delete generic gdi objects.
 * After an object was modified/added successfully it
 * is necessary to spool the current state to the filesystem.
 *
 * @param alpp reference to an answer list.
 * @param ep rqs object which should be spooled
 * @param object structure of the gdi framework which contains additional information to perform the request (function pointers, names, CULL-types)
 * @param packet the client request
 * @param task the GDI task being answered
 *
 * @return [alpp] - error messages will be added to this list 0 - success STATUS_EEXIST - an error occurred
 *
 * @note MT-NOTE: rqs_spool() is MT safe
 */
int
rqs_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *ep, gdi_object_t *object) {
   DENTER(TOP_LAYER);

   lList *answer_list = nullptr;
   bool dbret;

   dbret = spool_write_object(&answer_list, spool_get_default_context(), ep,
                              lGetString(ep, RQS_name), SGE_TYPE_RQS, true);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S, lGetString(ep, RQS_name));
   }

   DRETURN(dbret ? 0 : 1);
}

/**
 * @brief Does something after a successful modify
 *
 * This function will be called from the framework which will
 * add/modify/delete generic gdi objects.
 * After an object was modified/added and spooled successfully
 * it is possibly necessary to perform additional tasks.
 * For example it is necessary to send some events to
 * other daemon.
 *
 * @param ep new rqs object
 * @param old_ep old rqs object before modification or nullptr if a new object was added
 * @param object structure of the gdi framework which contains additional information to perform the request (function pointers, names, CULL-types)
 * @param ppList
 * @param monitor monitoring structure
 * @param packet the client request
 * @param task the GDI task being answered
 *
 * @return 0 success
 *
 * @note MT-NOTE: rqs() is MT safe
 */
int
rqs_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lListElem *old_ep, gdi_object_t *object, lList **ppList, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   const char *rqs_name = lGetString(ep, RQS_name);
   rqs_update_categories(ep, old_ep, packet->gdi_session);
   sge_add_event(0, old_ep ? sgeE_RQS_MOD : sgeE_RQS_ADD, 0, 0, rqs_name, nullptr, nullptr, ep, packet->gdi_session);
   DRETURN(0);
}

/**
 * @brief Delete rqs object in master resource quota set list
 *
 * This function will be called from the framework which will
 * add/modify/delete generic gdi objects.
 * The purpose of this function is it to delete ckpt objects.
 *
 * @param ep element which should be deleted
 * @param alpp reference to an answer list.
 * @param rqs_list reference to the Master_RQS_LIST
 * @param ruser username of person who invoked this gdi request
 * @param rhost hostname of the host where someone initiated an gdi call
 * @param packet the client request
 * @param task the GDI task being answered
 *
 * @return success STATUS_EUNKNOWN - an error occurred
 *
 * @note MT-NOTE: rqs_del() is MT safe
 */
int
rqs_del(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lList **alpp, lList **rqs_list, char *ruser, char *rhost) {
   DENTER(TOP_LAYER);

   const char *rqs_name;
   int pos;
   lListElem *found;

   if (!ep || !ruser || !rhost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* ep is no rqs element, if ep has no RQS_name */
   if ((pos = lGetPosViaElem(ep, RQS_name, SGE_NO_ABORT)) < 0) {
      CRITICAL(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(RQS_name), __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   rqs_name = lGetPosString(ep, pos);
   if (!rqs_name) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* search for rqs with this name and remove it from the list */
   if (!(found = rqs_list_locate(*rqs_list, rqs_name))) {
      ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_RQS, rqs_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   found = lDechainElem(*rqs_list, found);

   rqs_update_categories(nullptr, found, packet->gdi_session);

   sge_event_spool(alpp, 0, sgeE_RQS_DEL, 0, 0, rqs_name, nullptr, nullptr,
                   nullptr, nullptr, nullptr, true, true, packet->gdi_session);

   INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, ruser, rhost, rqs_name, MSG_OBJ_RQS);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

   lFreeElem(&found);

   DRETURN(STATUS_OK);
}

/**
 * @brief Debit running jobs
 *
 * Newly added resource quota sets need to be debited for all running jos
 * This is done by this function
 *
 * @param rqs resource quota set (RQS_Type)
 * @param answer_list answer list
 *
 * @return always true
 *
 * @note MT-NOTE: rqs_reinit_consumable_actual_list() is not MT safe
 */
static bool
rqs_reinit_consumable_actual_list(lListElem *rqs, lList **answer_list) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const lList *master_centry_list = *ocs::DataStore::get_master_list(SGE_TYPE_CENTRY);
   const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
   const lList *master_hgroup_list = *ocs::DataStore::get_master_list(SGE_TYPE_HGROUP);
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);
   const lList *master_pe_list = *ocs::DataStore::get_master_list(SGE_TYPE_PE);

   if (rqs != nullptr) {
      const lListElem *rule = nullptr;

      for_each_ep(rule, lGetList(rqs, RQS_rule)) {
         for_each_rw_lv(limit, lGetList(rule, RQR_limit)) {
            lList *usage = nullptr;
            lXchgList(limit, RQRL_usage, &usage);
            lFreeList(&usage);
         }
      }

      if (!lGetBool(rqs, RQS_enabled)) {
         DRETURN(ret);
      }

      for_each_rw_lv(job, master_job_list) {
         const lList *ja_task_list = lGetList(job, JB_ja_tasks);
         const lListElem *ja_task = nullptr;

         for_each_ep(ja_task, ja_task_list) {
            const lList *gdi_list = lGetList(ja_task, JAT_granted_destin_identifier_list);
            bool is_master_task = true;

            const char *pe_name = lGetString(ja_task, JAT_granted_pe);
            const lListElem *pe = nullptr;
            if (pe_name != nullptr) {
               pe = pe_list_locate(master_pe_list, pe_name);
            }

            const lListElem *gdil_ep;
            const char *last_hostname = nullptr;
            for_each_ep(gdil_ep, gdi_list) {
               bool do_per_host_booking = host_do_per_host_booking(&last_hostname, lGetHost(gdil_ep, JG_qhostname));
               int tmp_slot = lGetUlong(gdil_ep, JG_slots);
               rqs_debit_consumable(rqs, job, gdil_ep, pe, master_centry_list,
                                    master_userset_list, master_hgroup_list, tmp_slot, is_master_task, do_per_host_booking);
               is_master_task = false;
            }
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief Diff single scope
 *
 * This function iterates over a scope and generates
 * a list with all referenced names.
 * This function resolves wildcards by using fnmatch for patterned scopes.
 * This function can only be used for usersets or projects
 *
 * @param filter_scope filter scope (RQRF_type)
 * @param filter_nm nm of the filter type (eg. RQR_filter_users)
 * @param scope_ref generated resolved list
 * @param nm nm of the names of the list
 * @param dp type of the generated list
 * @param master_list master list, needed for resolving
 *
 * @return true if one or more scopes where found false if all projects or usersets are referenced
 *
 * @note MT-NOTE: filter_diff_usersets_or_projects_scope() is MT safe
 *
 * @see #filter_diff_usersets_or_projects
 */
static bool
filter_diff_usersets_or_projects_scope(lList *filter_scope, int filter_nm, lList **scope_ref, int nm, const lDescr *dp,
                                       const lList *master_list) {
   DENTER(TOP_LAYER);

   const lListElem *scope_ep;
   const char *scope;
   bool ret = true;

   for_each_ep(scope_ep, filter_scope) {
      scope = lGetString(scope_ep, ST_name);
      if (filter_nm == RQR_filter_users) {
         if (!ocs::is_hgroup_name(scope)) {
            continue;
         } else {
            scope++; /* sge intern usergroups don't have the preleading @ sign */
         }
      }

      if (strcmp("*", scope) == 0) {
         lEnumeration *what = lWhat("%T(%I)", dp, nm);

         lFreeList(scope_ref);
         /* 
          * that looks strange: list is simply sge_free()'d 
          * however this is no bug since any entry contained 
          * in the old scope_ref list will be also in the new one 
          */
         *scope_ref = lSelect("", master_list, nullptr, what);
         lFreeWhat(&what);
         ret = false;
         break;
      } else {
         if (ocs::is_pattern(scope)) {
            const lListElem *ep;
            for_each_ep(ep, master_list) {
               const char *ep_entry = lGetString(ep, nm);
               if (fnmatch(scope, ep_entry, 0) == 0) {
                  if (lGetElemStr(*scope_ref, nm, scope) == nullptr) {
                     lAddElemStr(scope_ref, nm, ep_entry, dp);
                  }
               }
            }
         } else {
            if (lGetElemStr(*scope_ref, nm, scope) == nullptr) {
               lAddElemStr(scope_ref, nm, scope, dp);
            }
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief Generate list of referenced usersets
 *
 * This function iterates over the project of user scope of a rule and generates
 * a list with all referenced names.
 * This function resolves wildcards by using fnmatch for patterned scopes.
 * This function can only be used for usersets or projects
 *
 * @param rule resource quota rule (RQR_Type)
 * @param filter_nm nm of the filter type (eg. RQR_filter_users)
 * @param scope_l generated resolved list
 * @param nm nm of the names of the list
 * @param dp type of the generated list
 * @param master_list master list, needed for resolving
 *
 * @return true if one or more scopes where found false if all projects or usersets are referenced
 *
 * @note MT-NOTE: filter_diff_usersets_or_projects() is MT safe
 */
static bool
filter_diff_usersets_or_projects(const lListElem *rule, int filter_nm, lList **scope_l, int nm, const lDescr *dp,
                                 const lList *master_list) {
   DENTER(TOP_LAYER);

   lListElem *filter;
   bool ret = true;

   if (filter_nm != RQR_filter_users && filter_nm != RQR_filter_projects) {
      DRETURN(ret);
   }

   if (rule == nullptr || master_list == nullptr) {
      DRETURN(ret);
   }

   if ((filter = lGetObject(rule, filter_nm)) == nullptr) {
      DRETURN(ret);
   }

   if ((ret = filter_diff_usersets_or_projects_scope(lGetListRW(filter, RQRF_scope), filter_nm, scope_l, nm, dp,
                                                     master_list))) {
      ret = filter_diff_usersets_or_projects_scope(lGetListRW(filter, RQRF_xscope), filter_nm, scope_l, nm, dp,
                                                   master_list);
   }

   DRETURN(ret);
}

/**
 * @brief Diff referenced usersets in rqs
 *
 * This function generates a list of all usersets referenced in a resource quota set.
 * After locating the usersets a diff beween the usersets found in old_list and new_list
 * is done and the usersets referenced in both are removed.
 *
 * @param new_rqs new resource quota set list (RQS_Type)
 * @param old_rqs old resource quota set list (RQS_Type)
 * @param new_list list of referenced usersets in new_rqs (US_Type)
 * @param old_list list of referenced usersets in old_rqs (US_Type)
 * @param master_userset_list see the brief above
 *
 * @return true if some or none userset is referenced false if all userset are referenced in new_list
 *
 * @note MT-NOTE: rqs_diff_usersets() is MT safe
 *
 * @see #rqs_diff_projects
 */
bool
rqs_diff_usersets(const lListElem *new_rqs, const lListElem *old_rqs, lList **new_list, lList **old_list,
                  const lList *master_userset_list) {
   DENTER(TOP_LAYER);

   const lListElem *rule;
   bool ret = true;

   if (old_rqs && old_list) {
      for_each_ep(rule, lGetList(old_rqs, RQS_rule)) {
         if (!filter_diff_usersets_or_projects(rule, RQR_filter_users, old_list, US_name, US_Type,
                                               master_userset_list)) {
            break;
         }
      }
   }

   if (new_rqs && new_list) {
      for_each_ep(rule, lGetList(new_rqs, RQS_rule)) {
         if (!filter_diff_usersets_or_projects(rule, RQR_filter_users, new_list, US_name, US_Type,
                                               master_userset_list)) {
            if (!old_rqs || lGetNumberOfElem(*old_list) == 0) {
               ret = false;
            }
            break;
         }
      }
   }
   lDiffListStr(US_name, new_list, old_list);

   DRETURN(ret);
}

/**
 * @brief Diff referenced usersets in rqs
 *
 * This function generates a list of all projects referenced in a resource quota set.
 * After locating the projects a diff beween the projects found in old_list and new_list
 * is done and the projects referenced in both are removed.
 *
 * @param new_rqs new resource quota set list (RQS_Type)
 * @param old_rqs old resource quota set list (RQS_Type)
 * @param new_list list of referenced projects in new_rqs (PR_Type)
 * @param old_list list of referenced projects in old_rqs (PR_Type)
 * @param master_project_list see the brief above
 *
 * @return true if some or none project is referenced false if all projects are referenced in new_list
 *
 * @note MT-NOTE: rqs_diff_projects() is MT safe
 *
 * @see #rqs_diff_usersets
 */
bool
rqs_diff_projects(const lListElem *new_rqs, const lListElem *old_rqs, lList **new_list, lList **old_list,
                  const lList *master_project_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (old_rqs && old_list) {
      const lListElem *rule;
      for_each_ep(rule, lGetList(old_rqs, RQS_rule)) {
         if (!filter_diff_usersets_or_projects(rule, RQR_filter_projects, old_list, PR_name, PR_Type,
                                               master_project_list)) {
            break;
         }
      }
   }

   if (new_rqs && new_list) {
      const lListElem *rule;
      for_each_ep(rule, lGetList(new_rqs, RQS_rule)) {
         if (!filter_diff_usersets_or_projects(rule, RQR_filter_projects, new_list, PR_name, PR_Type,
                                               master_project_list)) {
            if (!old_rqs || lGetNumberOfElem(*old_list) == 0) {
               ret = false;
            }
            break;
         }
      }
   }

   lDiffListStr(PR_name, new_list, old_list);

   DRETURN(ret);
}

/**
 * @brief Update categories after rqs change
 *
 * This function generates a list of referenced usersets and projects by the
 * new and the old resource quota set and updates the "consider_with_categories"
 * flag for the relevant objects.
 *
 * @param new_rqs new resource quota set (RQS_Type)
 * @param old_rqs old resource quota set (RQS_Type)
 *
 * @note MT-NOTE: rqs_update_categories() is not MT safe
 */
static void
rqs_update_categories(const lListElem *new_rqs, const lListElem *old_rqs, uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   lList *old_lp = nullptr, *new_lp = nullptr;
   const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
   const lList *master_project_list = *ocs::DataStore::get_master_list(SGE_TYPE_PROJECT);

   rqs_diff_projects(new_rqs, old_rqs, &new_lp, &old_lp, master_project_list);
   project_update_categories(new_lp, old_lp, gdi_session);
   lFreeList(&old_lp);
   lFreeList(&new_lp);

   rqs_diff_usersets(new_rqs, old_rqs, &new_lp, &old_lp, master_userset_list);
   userset_update_categories(new_lp, old_lp, gdi_session);
   lFreeList(&old_lp);
   lFreeList(&new_lp);

   DRETURN_VOID;
}

/**
 * @brief Check if the name is referenced in the resource
 *
 * This function iterates over all rules of a rule set and check if the given name
 * is referenced in the scope list of the rules.
 * The compare is done with fnmatch(), so wildcards are allowed.
 * userset note:
 * usersets in the resource quota sets have a preleading @ like hostgroups. This must be
 * consideres if usersets are searched with this function. If the name has no preleading @
 * this function searches for users instead of usersets.
 *
 * @param rqs resource quota set to search (RQS_Type)
 * @param nm type of the filter scope
 * @param name name so search
 *
 * @return true if name is referenced false if no reference was found
 *
 * @note MT-NOTE: scope_is_referenced_rqs() is MT safe
 */
bool
scope_is_referenced_rqs(const lListElem *rqs, int nm, const char *name) {
   DENTER(TOP_LAYER);

   const lListElem *rule;
   bool ret = false;

   if (rqs == nullptr || name == nullptr) {
      DRETURN(ret);
   }

   for_each_ep(rule, lGetList(rqs, RQS_rule)) {
      lListElem *filter = lGetObject(rule, nm);
      if (filter != nullptr) {
         const lListElem *scope_ep;
         for_each_ep(scope_ep, lGetList(filter, RQRF_scope)) {
            const char *scope = lGetString(scope_ep, ST_name);
            if (fnmatch(scope, name, 0) == 0) {
               ret = true;
               break;
            }
         }
         if (ret) {
            break;
         }
         for_each_ep(scope_ep, lGetList(filter, RQRF_xscope)) {
            const char *scope = lGetString(scope_ep, ST_name);
            if (fnmatch(scope, name, 0) == 0) {
               ret = true;
               break;
            }
         }
         if (ret) {
            break;
         }
      }
   }

   DRETURN(ret);
}
