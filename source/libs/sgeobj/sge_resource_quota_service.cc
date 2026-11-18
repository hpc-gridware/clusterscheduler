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
 *  Portions of this software are Copyright (c) 2025-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Queries against the resource quota sets
 *
 * @see sge_resource_quota_service.h
 */

#include <cstring>
#include <climits>

#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"

#include "sched/sge_select_queue.h"
#include "sched/sort_hosts.h"

#include "sge_str.h"
#include "sge_resource_quota.h"
#include "sge_resource_quota_service.h"
#include "sge_object.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_RQR.h"
#include "sgeobj/tom/ocs_tom_RQRF.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"

/**
 * @brief Does a filter of a rule leave its scope open?
 *
 * A filter is global when it is missing, or when it names `*` and excludes
 * nothing.
 *
 * @tparam F the filter field of the rule (RQR_filter_*)
 * @param rule the rule (RQR_Type)
 * @return true when the filter puts no restriction on the scope
 */
template <auto F>
static bool is_global(const ocs::tom::ConstElement<ocs::tom::RQR> rule) {
   // no DENTER: called per job/queue in the scheduler
   const auto filter = ocs::tom::get<F>(rule);
   if (!filter) {
      return true;
   }
   if (ocs::tom::find_first_in<RQRF_scope, ST_name>(filter, "*") && ocs::tom::get<RQRF_xscope>(filter).empty()) {
      return true;
   }
   return false;
}

/**
 * @brief Global rule with regards to cluster queues?
 *
 * @param rule RQR_Type
 *
 * @return True if cluster queues play no role with the rule
 *
 * @note MT-NOTE: is_cqueue_global() is MT safe
 */
bool is_cqueue_global(const ocs::tom::ConstElement<ocs::tom::RQR> rule) {
   // no DENTER: called per job/queue in the scheduler
   return is_global<RQR_filter_queues>(rule);
}


/**
 * @brief Global rule with regards to hosts?
 *
 * Return true if hosts play no role with the rule
 *
 * @param rule RQR_Type
 *
 * @return True if hosts play no role with the rule
 *
 * @note MT-NOTE: is_host_global() is MT safe
 */
bool is_host_global(const ocs::tom::ConstElement<ocs::tom::RQR> rule) {
   // no DENTER: called per job/queue in the scheduler
   return is_global<RQR_filter_hosts>(rule);
}

/**
 * @brief Does a filter of a rule expand?
 *
 * @tparam F the filter field of the rule (RQR_filter_*)
 * @param rule the rule (RQR_Type)
 * @return true when the filter exists and is written in braces (`{...}`)
 */
template <auto F>
static bool is_expand(const ocs::tom::ConstElement<ocs::tom::RQR> rule) {
   // no DENTER: called per job/queue in the scheduler
   const auto filter = ocs::tom::get<F>(rule);
   return filter && ocs::tom::get<RQRF_expand>(filter);
}


/**
 * @brief Returns true if rule expands on hosts
 *
 * Returns true if rule expands on hosts.
 *
 * @code
 *  "hosts {*}" returns true
 *  "hosts @allhosts" returns false
 * @endcode
 *
 * @param rule RQR_Type
 *
 * @return True if rule expands on hosts
 *
 * @note MT-NOTE: is_host_expand() is MT safe
 */
bool is_host_expand(const ocs::tom::ConstElement<ocs::tom::RQR> rule) {
   // no DENTER: called per job/queue in the scheduler
   return is_expand<RQR_filter_hosts>(rule);
}

/**
 * @brief Returns true if rule expands on cluster queues
 *
 * Returns true if rule expands on cluster queues.
 *
 * @code
 *  "queues {*}" returns true
 *  "queues Q001,Q002" returns false
 * @endcode
 *
 * @param rule RQR_Type
 *
 * @return True if rule expands on hosts
 *
 * @note MT-NOTE: is_cqueue_expand() is MT safe
 */
bool is_cqueue_expand(const ocs::tom::ConstElement<ocs::tom::RQR> rule) {
   // no DENTER: called per job/queue in the scheduler
   return is_expand<RQR_filter_queues>(rule);
}


/**
 * @brief Search for user reference in rqs
 *
 * Search for a user reference in the resource quota sets
 *
 * @param rqs resource quota set list (RQS_Type)
 * @param user user to search
 * @param group user's group
 * @param grp_list the user's supplementary groups (ST_Type), may be an empty handle
 * @param acl_list acl list for user resolving (US_Type)
 *
 * @return true if user was found false if user was not found
 *
 * @note MT-NOTE: sge_user_is_referenced_in_rqs() is MT safe
 */
bool sge_user_is_referenced_in_rqs(const ocs::tom::ConstList<ocs::tom::RQS> rqs, const char *user, const char *group,
                                   const ocs::tom::ConstList<ocs::tom::ST> grp_list,
                                   const ocs::tom::ConstList<ocs::tom::US> acl_list) {
   DENTER(BASIS_LAYER);

   const ocs::tom::ConstList<ocs::tom::HGRP> no_hgroups;
   bool ret = false;

   for (const auto ep : rqs) {
      for (const auto rule : ocs::tom::get<RQS_rule>(ep)) {
         /* there may be no per-user limitation and also not limitation that is special for this user */
         if ((is_expand<RQR_filter_users>(rule) || !is_global<RQR_filter_users>(rule)) &&
             rqs_filter_match(ocs::tom::get<RQR_filter_users>(rule), FILTER_USERS, user, acl_list, no_hgroups, group,
                              grp_list)) {
            ret = true;
            break;
         }
      }
      if (ret) {
         break;
      }
   }
   DRETURN(ret);
}

