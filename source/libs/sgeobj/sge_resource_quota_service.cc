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

static bool is_global(const lListElem *rule, int nm)
{
   lListElem *filter = lGetObject(rule, nm);
   if (!filter)
      return true;
   if (lGetSubStr(filter, ST_name, "*", RQRF_scope) && lGetNumberOfElem(lGetList(filter, RQRF_xscope))==0)
      return true;
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
bool is_cqueue_global(const lListElem *rule)
{
   return is_global(rule, RQR_filter_queues);
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
bool is_host_global(const lListElem *rule)
{
   return is_global(rule, RQR_filter_hosts);
}

static bool is_expand(const lListElem *rule, int nm)
{
   lListElem *filter = lGetObject(rule, nm);
   if (filter && lGetBool(filter, RQRF_expand))
      return true;
   else
      return false;
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
bool is_host_expand(const lListElem *rule)
{
   return is_expand(rule, RQR_filter_hosts);
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
bool is_cqueue_expand(const lListElem *rule)
{
   return is_expand(rule, RQR_filter_queues);
}


/**
 * @brief Search for user reference in rqs
 *
 * Search for a user reference in the resource quota sets
 *
 * @param rqs resource quota set list
 * @param user user to search
 * @param group user's group
 * @param grp_list the user's supplementary groups
 * @param acl_list acl list for user resolving
 *
 * @return true if user was found false if user was not found
 *
 * @note MT-NOTE: sge_user_is_referenced_in_rqs() is MT safe
 */
bool sge_user_is_referenced_in_rqs(const lList *rqs, const char *user, const char *group, const lList *grp_list, const lList *acl_list)
{
   bool ret = false;

   for_each_ep_lv(ep, rqs) {
      const lList *rule_list = lGetList(ep, RQS_rule);

      for_each_ep_lv(rule, rule_list) {
         /* there may be no per-user limitation and also not limitation that is special for this user */
         if ((is_expand(rule, RQR_filter_users) || !is_global(rule, RQR_filter_users)) &&
             rqs_filter_match(lGetObject(rule, RQR_filter_users), FILTER_USERS, user, acl_list, nullptr, group, grp_list)) {
            ret = true;
            break;
         }
      }
      if (ret) {
         break;
      }
   }
   return ret;
}
