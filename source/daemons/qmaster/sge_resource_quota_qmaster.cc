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
#include <utility>

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
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Select.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_RQR.h"
#include "sgeobj/tom/ocs_tom_RQRF.h"
#include "sgeobj/tom/ocs_tom_RQRL.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"

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
rqs_reinit_consumable_actual_list(ocs::tom::Element<ocs::tom::RQS> rqs, ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

static void
rqs_update_categories(ocs::tom::ConstElement<ocs::tom::RQS> new_rqs, ocs::tom::ConstElement<ocs::tom::RQS> old_rqs,
                      uint64_t gdi_session);

/**
 * @brief Gdi callback function for modifing resource quota sets
 *
 * This function is called from the framework that
 * add/modify/delete generic gdi objects.
 * The purpose of this function is it to add new rqs
 * objects or modify existing resource quota sets.
 * The raw parameters are fixed by modifier_func_t; both elements are of RQS_Type.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp reference to an answer list
 * @param new_rqs if a new rqs object will be created by this function, then new_rqs is a newly initialized
 *        CULL object. if this function was called due to a modify request than new_rqs will contain the old data
 * @param rqs a reduced rqs object which contains all necessary information to create a new object or modify
 *        parts of an existing one
 * @param add 1 if a new element should be added to the master list 0 to modify an existing object
 * @param ruser username who invoked this gdi request
 * @param rhost hostname of where the gdi request was invoked
 * @param object structure of the gdi framework which contains additional information to perform the request
 * @param cmd the command being executed
 * @param sub_command how should we handle sublist elements: `SGE_GDI_CHANGE`
 *        modifies them, `SGE_GDI_APPEND` adds to the sublist, `SGE_GDI_REMOVE`
 *        removes from it, `SGE_GDI_SET` replaces the whole sublist
 * @param monitor monitoring structure
 *
 * @return 0 on success STATUS_EUNKNOWN if an error occurred
 *
 * @note MT-NOTE: rqs_mod() is MT safe
 */
int
rqs_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp, lListElem *new_rqs,
        lListElem *rqs, int add, const char *ruser, const char *rhost, gdi_object_t *object, ocs::gdi::Command cmd,
        ocs::gdi::SubCommand sub_command, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const ocs::tom::Element<ocs::tom::RQS> target(new_rqs);
   const ocs::tom::Element<ocs::tom::RQS> delta(rqs);
   const char *rqs_name = nullptr;
   bool rules_changed = false;
   bool previous_enabled = ocs::tom::get<RQS_enabled>(target);
   const ocs::tom::ConstList<ocs::tom::CE> master_centry_list(*ocs::DataStore::get_master_list(SGE_TYPE_CENTRY));

   /* ---- RQS_name */
   if (add) {
      if (attr_mod_str(alpp, delta, target, RQS_name, object->object_name))
         goto ERROR;
   }
   rqs_name = ocs::tom::get<RQS_name>(target);

   /* Name has to be a valid name */
   if (add && verify_obj_name(alpp, rqs_name, MAX_VERIFY_STRING,
                              MSG_OBJ_RQS) != STATUS_OK) {
      goto ERROR;
   }

   /* ---- RQS_description */
   attr_mod_zerostr(delta, target, RQS_description, "description");

   /* ---- RQS_enabled */
   attr_mod_bool(delta, target, RQS_enabled, "enabled");

   /* ---- RQS_rule */
   if (ocs::tom::has<RQS_rule>(delta)) {
      rules_changed = true;
      if ((sub_command & ocs::gdi::SubCommand::SET_ALL) == ocs::gdi::SubCommand::SET_ALL) {
         normalize_sublist(delta, RQS_rule);
         attr_mod_sub_list(alpp, target, RQS_rule, RQS_name, delta, cmd, sub_command,
                           SGE_ATTR_RQSRULES, SGE_OBJ_RQS, 0, nullptr);
      } else {
         /* *attr cases */
         for (const auto rule : ocs::tom::get<RQS_rule>(delta)) {
            const auto new_rule = rqs_rule_locate(ocs::tom::get<RQS_rule>(target), ocs::tom::get<RQR_name>(rule));
            if (new_rule) {
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

   if (!rqs_verify_attributes(target, alpp, true, master_centry_list)) {
      goto ERROR;
   }
   if (rules_changed || (ocs::tom::get<RQS_enabled>(target) != previous_enabled)) {
      rqs_reinit_consumable_actual_list(target, alpp);
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
 * The raw parameters are fixed by writer_func_t; the element is of RQS_Type.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp reference to an answer list, error messages will be added to this list
 * @param ep rqs object which should be spooled
 * @param object structure of the gdi framework which contains additional information to perform the request
 *        (function pointers, names, CULL-types)
 *
 * @return 0 - success, 1 - an error occurred
 *
 * @note MT-NOTE: rqs_spool() is MT safe
 */
int
rqs_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp, lListElem *ep,
          gdi_object_t *object) {
   DENTER(TOP_LAYER);

   const ocs::tom::ConstElement<ocs::tom::RQS> rqs(ep);
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;

   const bool dbret = spool_write_object(&answer_list, spool_get_default_context(), rqs,
                                         ocs::tom::get<RQS_name>(rqs), SGE_TYPE_RQS);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S, ocs::tom::get<RQS_name>(rqs));
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
 * other daemon. The raw parameters are fixed by on_success_func_t; the
 * elements are of RQS_Type.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param ep new rqs object
 * @param old_ep old rqs object before modification or nullptr if a new object was added
 * @param object structure of the gdi framework which contains additional information to perform the request
 *        (function pointers, names, CULL-types)
 * @param ppList receives information for post processing, unused
 * @param monitor monitoring structure
 *
 * @return 0 success
 *
 * @note MT-NOTE: rqs() is MT safe
 */
int
rqs_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lListElem *old_ep, gdi_object_t *object,
            lList **ppList, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   const ocs::tom::Element<ocs::tom::RQS> rqs(ep);
   const char *rqs_name = ocs::tom::get<RQS_name>(rqs);
   rqs_update_categories(rqs, ocs::tom::ConstElement<ocs::tom::RQS>(old_ep), packet->gdi_session);
   sge_add_event(0, old_ep ? sgeE_RQS_MOD : sgeE_RQS_ADD, 0, 0, rqs_name, nullptr, nullptr, rqs, packet->gdi_session);
   DRETURN(0);
}

/**
 * @brief Delete rqs object in master resource quota set list
 *
 * This function will be called from the framework which will
 * add/modify/delete generic gdi objects.
 * The purpose of this function is it to delete rqs objects.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param ep element which should be deleted
 * @param alpp reference to an answer list.
 * @param rqs_list the master resource quota set list
 * @param ruser username of person who invoked this gdi request
 * @param rhost hostname of the host where someone initiated an gdi call
 *
 * @return STATUS_OK on success, STATUS_EUNKNOWN or STATUS_EEXIST if an error occurred
 *
 * @note MT-NOTE: rqs_del() is MT safe
 */
int
rqs_del(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::ConstElement<ocs::tom::RQS> ep,
        ocs::tom::ListPtr<ocs::tom::AN> *alpp,
        const ocs::tom::List<ocs::tom::RQS> rqs_list, const char *ruser, const char *rhost) {
   DENTER(TOP_LAYER);

   if (!ep || !ruser || !rhost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* ep is no rqs element, if ep has no RQS_name */
   if (!ep.has<RQS_name>()) {
      CRITICAL(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(RQS_name), __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   const char *rqs_name = ocs::tom::get<RQS_name>(ep);
   if (!rqs_name) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* search for rqs with this name and remove it from the list */
   const auto found = rqs_list_locate(rqs_list, rqs_name);
   if (!found) {
      ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_RQS, rqs_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   auto removed = rqs_list.extract(found);

   rqs_update_categories(nullptr, *removed, packet->gdi_session);

   sge_event_spool(alpp, 0, sgeE_RQS_DEL, 0, 0, rqs_name, nullptr, nullptr,
                   nullptr, nullptr, nullptr, true, true, packet->gdi_session);

   INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, ruser, rhost, rqs_name, MSG_OBJ_RQS);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

   removed.reset();

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
rqs_reinit_consumable_actual_list(const ocs::tom::Element<ocs::tom::RQS> rqs,
                                  ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const ocs::tom::ConstList<ocs::tom::CE> master_centry_list(*ocs::DataStore::get_master_list(SGE_TYPE_CENTRY));
   const ocs::tom::ConstList<ocs::tom::US> master_userset_list(*ocs::DataStore::get_master_list(SGE_TYPE_USERSET));
   const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list(*ocs::DataStore::get_master_list(SGE_TYPE_HGROUP));
   const ocs::tom::ConstList<ocs::tom::JB> master_job_list(*ocs::DataStore::get_master_list(SGE_TYPE_JOB));
   const ocs::tom::ConstList<ocs::tom::PE> master_pe_list(*ocs::DataStore::get_master_list(SGE_TYPE_PE));

   if (rqs) {
      for (const auto rule : ocs::tom::get<RQS_rule>(rqs)) {
         for (const auto limit : ocs::tom::get<RQR_limit>(rule)) {
            ocs::tom::set<RQRL_usage>(limit, ocs::tom::ListPtr<ocs::tom::RUE>());
         }
      }

      if (!ocs::tom::get<RQS_enabled>(rqs)) {
         DRETURN(ret);
      }

      for (const auto job : master_job_list) {
         for (const auto ja_task : ocs::tom::get<JB_ja_tasks>(job)) {
            bool is_master_task = true;

            const char *pe_name = ocs::tom::get<JAT_granted_pe>(ja_task);
            ocs::tom::ConstElement<ocs::tom::PE> pe;
            if (pe_name != nullptr) {
               pe = pe_list_locate(master_pe_list, pe_name);
            }

            const char *last_hostname = nullptr;
            for (const auto gdil_ep : ocs::tom::get<JAT_granted_destin_identifier_list>(ja_task)) {
               bool do_per_host_booking = host_do_per_host_booking(&last_hostname,
                                                                   ocs::tom::get<JG_qhostname>(gdil_ep));
               int tmp_slot = ocs::tom::get<JG_slots>(gdil_ep);
               rqs_debit_consumable(rqs, job, gdil_ep, pe, master_centry_list,
                                    master_userset_list, master_hgroup_list, tmp_slot, is_master_task,
                                    do_per_host_booking);
               is_master_task = false;
            }
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief The filter of a rule for a filter field known at run time
 * @param rule the rule (RQR_Type)
 * @param filter_nm the filter field, one of RQR_filter_users, _projects, _pes, _queues and _hosts
 * @return the filter, or an empty handle if the rule has none or the field is no filter field
 */
static ocs::tom::ConstElement<ocs::tom::RQRF>
ocs_rqr_filter(const ocs::tom::ConstElement<ocs::tom::RQR> rule, const int filter_nm) {
   DENTER(TOP_LAYER);

   ocs::tom::ConstElement<ocs::tom::RQRF> filter;
   switch (filter_nm) {
      case RQR_filter_users:
         filter = ocs::tom::get<RQR_filter_users>(rule);
         break;
      case RQR_filter_projects:
         filter = ocs::tom::get<RQR_filter_projects>(rule);
         break;
      case RQR_filter_pes:
         filter = ocs::tom::get<RQR_filter_pes>(rule);
         break;
      case RQR_filter_queues:
         filter = ocs::tom::get<RQR_filter_queues>(rule);
         break;
      case RQR_filter_hosts:
         filter = ocs::tom::get<RQR_filter_hosts>(rule);
         break;
      default:
         break;
   }

   DRETURN(filter);
}

/**
 * @brief Remove the names two lists share from both, freeing a list emptied by that, as lDiffListStr() did
 * @tparam T the object type
 * @tparam F the name field
 * @param lp1 the first list, an empty handle removes nothing
 * @param lp2 the second list, an empty handle removes nothing
 */
template <typename T, auto F>
static void
ocs_diff_list_str(ocs::tom::ListPtr<T> &lp1, ocs::tom::ListPtr<T> &lp2) {
   DENTER(TOP_LAYER);

   if (lp1 && lp2) {
      const std::size_t before = lp1.get().size();
      lp1->template remove_common<F>(lp2.get());
      if (lp1.get().size() != before) {
         if (lp1.get().empty()) {
            lp1.reset();
         }
         if (lp2.get().empty()) {
            lp2.reset();
         }
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Diff single scope
 *
 * This function iterates over a scope and generates
 * a list with all referenced names.
 * This function resolves wildcards by using fnmatch for patterned scopes.
 * This function can only be used for usersets or projects
 *
 * @tparam T the object type of the generated list, US or PR
 * @tparam F the name field of @p T
 * @tparam FILTER the filter field, RQR_filter_users or RQR_filter_projects
 * @param filter_scope filter scope (ST_Type)
 * @param scope_ref generated resolved list
 * @param master_list master list, needed for resolving
 *
 * @return true if one or more scopes where found false if all projects or usersets are referenced
 *
 * @note MT-NOTE: filter_diff_usersets_or_projects_scope() is MT safe
 *
 * @see #filter_diff_usersets_or_projects
 */
template <typename T, auto F, auto FILTER>
static bool
filter_diff_usersets_or_projects_scope(const ocs::tom::ConstList<ocs::tom::ST> filter_scope,
                                       ocs::tom::ListPtr<T> &scope_ref, const ocs::tom::ConstList<T> master_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   for (const auto scope_ep : filter_scope) {
      const char *scope = ocs::tom::get<ST_name>(scope_ep);
      if constexpr (FILTER == RQR_filter_users) {
         if (!ocs::is_hgroup_name(scope)) {
            continue;
         } else {
            scope++; /* sge intern usergroups don't have the preleading @ sign */
         }
      }

      if (strcmp("*", scope) == 0) {
         /* 
          * that looks strange: list is simply sge_free()'d 
          * however this is no bug since any entry contained 
          * in the old scope_ref list will be also in the new one 
          */
         scope_ref = master_list.select(ocs::tom::what<F>(), "");
         ret = false;
         break;
      } else {
         if (ocs::is_pattern(scope)) {
            for (const auto ep : master_list) {
               const char *ep_entry = ocs::tom::get<F>(ep);
               if (fnmatch(scope, ep_entry, 0) == 0) {
                  if (!ocs::tom::find_first<F>(scope_ref.get(), scope)) {
                     if (!scope_ref) {
                        scope_ref = ocs::tom::ListPtr<T>::create("");
                     }
                     scope_ref->template emplace_back<F>(ep_entry);
                  }
               }
            }
         } else {
            if (!ocs::tom::find_first<F>(scope_ref.get(), scope)) {
               if (!scope_ref) {
                  scope_ref = ocs::tom::ListPtr<T>::create("");
               }
               scope_ref->template emplace_back<F>(scope);
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
 * @tparam T the object type of the generated list, US or PR
 * @tparam F the name field of @p T
 * @tparam FILTER the filter field, RQR_filter_users or RQR_filter_projects
 * @param rule resource quota rule (RQR_Type)
 * @param scope_l generated resolved list
 * @param master_list master list, needed for resolving
 *
 * @return true if one or more scopes where found false if all projects or usersets are referenced
 *
 * @note MT-NOTE: filter_diff_usersets_or_projects() is MT safe
 */
template <typename T, auto F, auto FILTER>
static bool
filter_diff_usersets_or_projects(const ocs::tom::ConstElement<ocs::tom::RQR> rule, ocs::tom::ListPtr<T> &scope_l,
                                 const ocs::tom::ConstList<T> master_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (!rule || !master_list) {
      DRETURN(ret);
   }

   const auto filter = ocs::tom::get<FILTER>(rule);
   if (!filter) {
      DRETURN(ret);
   }

   if ((ret = filter_diff_usersets_or_projects_scope<T, F, FILTER>(ocs::tom::get<RQRF_scope>(filter), scope_l,
                                                                   master_list))) {
      ret = filter_diff_usersets_or_projects_scope<T, F, FILTER>(ocs::tom::get<RQRF_xscope>(filter), scope_l,
                                                                 master_list);
   }

   DRETURN(ret);
}

/**
 * @brief Diff referenced usersets or projects in rqs
 *
 * @tparam T the object type of the generated lists, US or PR
 * @tparam F the name field of @p T
 * @tparam FILTER the filter field, RQR_filter_users or RQR_filter_projects
 * @param new_rqs new resource quota set (RQS_Type), or an empty handle
 * @param old_rqs old resource quota set (RQS_Type), or an empty handle
 * @param new_list list of referenced objects in new_rqs, or nullptr
 * @param old_list list of referenced objects in old_rqs, or nullptr
 * @param master_list the master list of @p T
 *
 * @return true if some or none object is referenced false if all objects are referenced in new_list
 */
template <typename T, auto F, auto FILTER>
static bool
ocs_rqs_diff(const ocs::tom::ConstElement<ocs::tom::RQS> new_rqs, const ocs::tom::ConstElement<ocs::tom::RQS> old_rqs,
             ocs::tom::ListPtr<T> *new_list, ocs::tom::ListPtr<T> *old_list, const ocs::tom::ConstList<T> master_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (old_rqs && old_list) {
      for (const auto rule : ocs::tom::get<RQS_rule>(old_rqs)) {
         if (!filter_diff_usersets_or_projects<T, F, FILTER>(rule, *old_list, master_list)) {
            break;
         }
      }
   }

   if (new_rqs && new_list) {
      for (const auto rule : ocs::tom::get<RQS_rule>(new_rqs)) {
         if (!filter_diff_usersets_or_projects<T, F, FILTER>(rule, *new_list, master_list)) {
            if (!old_rqs || old_list->get().empty()) {
               ret = false;
            }
            break;
         }
      }
   }
   if (new_list != nullptr && old_list != nullptr) {
      ocs_diff_list_str<T, F>(*new_list, *old_list);
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
 * @param new_rqs new resource quota set (RQS_Type)
 * @param old_rqs old resource quota set (RQS_Type)
 * @param new_list list of referenced usersets in new_rqs (US_Type), or nullptr
 * @param old_list list of referenced usersets in old_rqs (US_Type), or nullptr
 * @param master_userset_list the master userset list, for resolving patterns
 *
 * @return true if some or none userset is referenced false if all userset are referenced in new_list
 *
 * @note MT-NOTE: rqs_diff_usersets() is MT safe
 *
 * @see #rqs_diff_projects
 */
bool
rqs_diff_usersets(const ocs::tom::ConstElement<ocs::tom::RQS> new_rqs,
                  const ocs::tom::ConstElement<ocs::tom::RQS> old_rqs, ocs::tom::ListPtr<ocs::tom::US> *new_list,
                  ocs::tom::ListPtr<ocs::tom::US> *old_list,
                  const ocs::tom::ConstList<ocs::tom::US> master_userset_list) {
   DENTER(TOP_LAYER);
   const bool ret = ocs_rqs_diff<ocs::tom::US, US_name, RQR_filter_users>(new_rqs, old_rqs, new_list, old_list,
                                                                           master_userset_list);
   DRETURN(ret);
}


/**
 * @brief Diff referenced projects in rqs
 *
 * This function generates a list of all projects referenced in a resource quota set.
 * After locating the projects a diff beween the projects found in old_list and new_list
 * is done and the projects referenced in both are removed.
 *
 * @param new_rqs new resource quota set (RQS_Type)
 * @param old_rqs old resource quota set (RQS_Type)
 * @param new_list list of referenced projects in new_rqs (PR_Type), or nullptr
 * @param old_list list of referenced projects in old_rqs (PR_Type), or nullptr
 * @param master_project_list the master project list, for resolving patterns
 *
 * @return true if some or none project is referenced false if all projects are referenced in new_list
 *
 * @note MT-NOTE: rqs_diff_projects() is MT safe
 *
 * @see #rqs_diff_usersets
 */
bool
rqs_diff_projects(const ocs::tom::ConstElement<ocs::tom::RQS> new_rqs,
                  const ocs::tom::ConstElement<ocs::tom::RQS> old_rqs, ocs::tom::ListPtr<ocs::tom::PR> *new_list,
                  ocs::tom::ListPtr<ocs::tom::PR> *old_list,
                  const ocs::tom::ConstList<ocs::tom::PR> master_project_list) {
   DENTER(TOP_LAYER);
   const bool ret = ocs_rqs_diff<ocs::tom::PR, PR_name, RQR_filter_projects>(new_rqs, old_rqs, new_list, old_list,
                                                                              master_project_list);
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
 * @param gdi_session the session the change belongs to
 *
 * @note MT-NOTE: rqs_update_categories() is not MT safe
 */
static void
rqs_update_categories(const ocs::tom::ConstElement<ocs::tom::RQS> new_rqs,
                      const ocs::tom::ConstElement<ocs::tom::RQS> old_rqs, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);

   const ocs::tom::ConstList<ocs::tom::US> master_userset_list(*ocs::DataStore::get_master_list(SGE_TYPE_USERSET));
   const ocs::tom::ConstList<ocs::tom::PR> master_project_list(*ocs::DataStore::get_master_list(SGE_TYPE_PROJECT));

   {
      ocs::tom::ListPtr<ocs::tom::PR> old_lp;
      ocs::tom::ListPtr<ocs::tom::PR> new_lp;
      rqs_diff_projects(new_rqs, old_rqs, &new_lp, &old_lp, master_project_list);
      project_update_categories(new_lp.get(), old_lp.get(), gdi_session);
   }

   {
      ocs::tom::ListPtr<ocs::tom::US> old_lp;
      ocs::tom::ListPtr<ocs::tom::US> new_lp;
      rqs_diff_usersets(new_rqs, old_rqs, &new_lp, &old_lp, master_userset_list);
      userset_update_categories(new_lp.get(), old_lp.get(), gdi_session);
   }

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
 * @param nm the filter field of the rules to search, e.g. RQR_filter_users
 * @param name name so search
 *
 * @return true if name is referenced false if no reference was found
 *
 * @note MT-NOTE: scope_is_referenced_rqs() is MT safe
 */
bool
scope_is_referenced_rqs(const ocs::tom::ConstElement<ocs::tom::RQS> rqs, const int nm, const char *name) {
   DENTER(TOP_LAYER);

   bool ret = false;

   if (!rqs || name == nullptr) {
      DRETURN(ret);
   }

   for (const auto rule : ocs::tom::get<RQS_rule>(rqs)) {
      const auto filter = ocs_rqr_filter(rule, nm);
      if (filter) {
         for (const auto scope_ep : ocs::tom::get<RQRF_scope>(filter)) {
            const char *scope = ocs::tom::get<ST_name>(scope_ep);
            if (fnmatch(scope, name, 0) == 0) {
               ret = true;
               break;
            }
         }
         if (ret) {
            break;
         }
         for (const auto scope_ep : ocs::tom::get<RQRF_xscope>(filter)) {
            const char *scope = ocs::tom::get<ST_name>(scope_ep);
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
