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

/****** sge_resource_quota_schedd/is_cqueue_global() ***************************
*  NAME
*     is_cqueue_global() -- Global rule with regards to cluster queues?
*
*  SYNOPSIS
*     bool is_cqueue_global(const lListElem *rule)
*
*  INPUTS
*     const lListElem *rule - RQR_Type
*
*  RESULT
*     bool - True if cluster queues play no role with the rule
*
*  NOTES
*     MT-NOTE: is_cqueue_global() is MT safe
*******************************************************************************/
bool is_cqueue_global(const lListElem *rule)
{
   return is_global(rule, RQR_filter_queues);
}


/****** sge_resource_quota_schedd/is_host_global() *****************************
*  NAME
*     is_host_global() -- Global rule with regards to hosts?
*
*  SYNOPSIS
*     bool is_host_global(const lListElem *rule)
*
*  FUNCTION
*     Return true if hosts play no role with the rule
*
*  INPUTS
*     const lListElem *rule - RQR_Type
*
*  RESULT
*     bool - True if hosts play no role with the rule
*
*  NOTES
*     MT-NOTE: is_host_global() is MT safe
*******************************************************************************/
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


/****** sge_resource_quota_schedd/is_host_expand() *****************************
*  NAME
*     is_host_expand() -- Returns true if rule expands on hosts
*
*  SYNOPSIS
*     bool is_host_expand(const lListElem *rule)
*
*  FUNCTION
*     Returns true if rule expands on hosts.
*
*  INPUTS
*     const lListElem *rule - RQR_Type
*
*  RESULT
*     bool - True if rule expands on hosts
*
*  EXAMPLE
*      "hosts {*}" returns true
*      "hosts @allhosts" returns false
*
*  NOTES
*     MT-NOTE: is_host_expand() is MT safe
*******************************************************************************/
bool is_host_expand(const lListElem *rule)
{
   return is_expand(rule, RQR_filter_hosts);
}

/****** sge_resource_quota_schedd/is_cqueue_expand() ***************************
*  NAME
*     is_cqueue_expand() -- Returns true if rule expands on cluster queues
*
*  SYNOPSIS
*     bool is_cqueue_expand(const lListElem *rule)
*
*  FUNCTION
*     Returns true if rule expands on cluster queues.
*
*  INPUTS
*     const lListElem *rule - RQR_Type
*
*  RESULT
*     bool - True if rule expands on hosts
*
*  EXAMPLE
*      "queues {*}" returns true
*      "queues Q001,Q002" returns false
*
*  NOTES
*     MT-NOTE: is_cqueue_expand() is MT safe
*******************************************************************************/
bool is_cqueue_expand(const lListElem *rule)
{
   return is_expand(rule, RQR_filter_queues);
}


/****** sge_resource_quota_schedd/sge_user_is_referenced_in_rqs() ********************
*  NAME
*     sge_user_is_referenced_in_rqs() -- search for user reference in rqs
*
*  SYNOPSIS
*     bool sge_user_is_referenced_in_rqs(const lList *rqs, const char *user,
*     lList *acl_list)
*
*  FUNCTION
*     Search for a user reference in the resource quota sets
*
*  INPUTS
*     const lList *rqs - resource quota set list
*     const char *user  - user to search
*     const char *group - user's group
*     lList *acl_list   - acl list for user resolving
*
*  RESULT
*     bool - true if user was found
*            false if user was not found
*
*  NOTES
*     MT-NOTE: sge_user_is_referenced_in_rqs() is MT safe
*
*******************************************************************************/
bool sge_user_is_referenced_in_rqs(const lList *rqs, const char *user, const char *group, const lList *grp_list, const lList *acl_list)
{
   bool ret = false;
   const lListElem *ep;

   for_each_ep(ep, rqs) {
      const lList *rule_list = lGetList(ep, RQS_rule);
      const lListElem *rule;

      for_each_ep(rule, rule_list) {
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
